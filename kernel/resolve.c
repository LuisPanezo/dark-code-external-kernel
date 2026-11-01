/*
 * phmeop — resolver de símbolos del kernel en runtime (para GKI).
 *
 * Problema: en GKI, kallsyms_lookup_name() y kallsyms_on_each_symbol() NO
 * están exportados a los módulos (se ocultaron en mainline 5.7, commit
 * "Unexport kallsyms_lookup_name() and kallsyms_on_each_symbol()"). Además
 * GKI filtra los EXPORT_SYMBOL a una lista KMI curada, así que register_kprobe
 * tampoco está garantizado.
 *
 * Solución (la misma que usa KernelSU/lkmloader.ko): localizar la dirección
 * de kallsyms_lookup_name() "caminando" la tabla kallsyms a través de
 * sprintf("%pS", addr):
 *
 *   - sprintf() sí es un símbolo exportado (y está en la KMI).
 *   - "%pS" resuelve una dirección a "<nombre>+0xOFF/0xSIZE" usando la tabla
 *     kallsyms interna del kernel (get_symbol_pos), sin necesidad de que el
 *     módulo acceda a kallsyms directamente.
 *   - Recorriendo los símbolos hacia atrás (direcciones decrecientes) desde la
 *     dirección de sprintf() se llega a kallsyms_lookup_name, que vive en
 *     kernel/kallsyms.c (enlazado antes que lib/vsprintf.c en el vmlinux).
 *   - Con kallsyms_lookup_name ya se resuelve cualquier otro símbolo,
 *     incluidos los de datos (CONFIG_KALLSYMS_ALL=y en este kernel).
 *
 * Referencias:
 *   https://5ec1cff.github.io/my-blog/2026/03/31/resolve-kallsyms-in-lkm/
 *   https://github.com/Kernel-SU/lkmloader.ko
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>

#include "resolve.h"

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

static kallsyms_lookup_name_t phmeop_lookup_name;

/* Convierte un literal hexadecimal ("0x2c", "0x2C", "2c") a unsigned long. */
static unsigned long phmeop_hex(const char *s)
{
	unsigned long v = 0;

	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s += 2;

	while (*s) {
		char c = *s;
		unsigned d;

		if (c >= '0' && c <= '9')
			d = (unsigned)(c - '0');
		else if (c >= 'a' && c <= 'f')
			d = (unsigned)(c - 'a') + 10;
		else if (c >= 'A' && c <= 'F')
			d = (unsigned)(c - 'A') + 10;
		else
			break;

		v = (v << 4) | d;
		s++;
	}
	return v;
}

/*
 * Camina la tabla kallsyms (vía sprintf "%pS") desde "start" buscando "target".
 * Devuelve la dirección base del símbolo, o 0 si no lo encuentra.
 */
static unsigned long phmeop_walk_find(const char *target,
				      unsigned long start, bool forward)
{
	char buf[128];
	unsigned long cur = forward ? start + 4 : start - 4;
	const unsigned long LIMIT = 32UL * 1024 * 1024; /* 32 MB */

	while ((forward ? (cur - start) : (start - cur)) < LIMIT) {
		char *plus, *slash, *sp;
		unsigned long off = 0, size = 4;

		if (sprintf(buf, "%pS", (void *)cur) <= 0)
			break;

		/* Sin símbolo (fuera del rango kernel): "0x..." -> fin. */
		if (buf[0] == '0' && buf[1] == 'x')
			break;

		/* Formato: "nombre+0xOFF/0xSIZE [modulo]" o "nombre+0xOFF/0xSIZE". */
		plus  = strchr(buf, '+');
		slash = strchr(buf, '/');
		sp    = strchr(buf, ' ');
		if (sp)
			*sp = '\0';                 /* quitar " [modulo]" */
		if (plus) {
			*plus = '\0';
			off = phmeop_hex(plus + 1);
		}
		if (slash)
			size = phmeop_hex(slash + 1);

		if (strcmp(buf, target) == 0)
			return cur - off;           /* base del símbolo */

		if (size == 0)
			size = 4;

		/* Saltar el símbolo completo para avanzar rápido. */
		if (forward)
			cur = (cur - off) + size + 4;
		else
			cur = (cur - off) - 4;
	}

	return 0;
}

static bool phmeop_lookup_name_init(void)
{
	unsigned long start, addr;

	if (phmeop_lookup_name)
		return true;

	/* &sprintf: símbolo exportado garantizado, punto de partida. */
	start = (unsigned long)&sprintf;

	/* kallsyms_lookup_name suele estar por debajo de sprintf (kernel/ se
	 * enlaza antes que lib/). Si no, probamos hacia arriba. */
	addr = phmeop_walk_find("kallsyms_lookup_name", start, false);
	if (!addr)
		addr = phmeop_walk_find("kallsyms_lookup_name", start, true);

	phmeop_lookup_name = (kallsyms_lookup_name_t)addr;
	if (!phmeop_lookup_name) {
		pr_err("phmeop: no se pudo localizar kallsyms_lookup_name\n");
		return false;
	}

	pr_info("phmeop: kallsyms_lookup_name en 0x%lx\n", addr);
	return true;
}

unsigned long phmeop_resolve_symbol(const char *name)
{
	if (!phmeop_lookup_name && !phmeop_lookup_name_init())
		return 0;
	return phmeop_lookup_name(name);
}
