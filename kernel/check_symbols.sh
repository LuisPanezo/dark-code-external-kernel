#!/usr/bin/env bash
# check_symbols.sh — verifica qué símbolos están exportados en tu GKI.
#
# Uso (en WSL2):
#   ./check_symbols.sh <Module.symvers | abi_symbollist>
#
# El archivo se baja de ci.android.com para tu build exacto, p. ej. build 12829524:
#   https://ci.android.com/builds/submitted/12829524/kernel_aarch64/latest/Module.symvers
#   (o el artefacto "abi_symbollist")
#
# Los símbolos de "REFERENCIA DIRECTA" deben aparecer OK; si alguno FALTA,
# el módulo no cargará por "Unknown symbol". Los de "SE RESUELVEN EN RUNTIME"
# se espera que FALTEN (por eso el driver los resuelve vía kallsyms).

set -u

if [ $# -ne 1 ]; then
  echo "Uso: $0 <Module.symvers o abi_symbollist>"
  exit 1
fi

SYMS="$1"
[ -f "$SYMS" ] || { echo "No existe: $SYMS"; exit 1; }

has() { grep -qE "(^|[[:space:]])$1([[:space:]]|$)" "$SYMS"; }

echo "== REFERENCIA DIRECTA (deben estar OK) =="
for s in sprintf printk misc_register misc_deregister no_llseek \
         copy_to_user copy_from_user strncpy_from_user \
         memcpy memset strchr strcmp strlen strstr; do
  if has "$s"; then echo "  OK     $s"; else echo "  FALTA  $s  <-- problema"; fi
done

echo
echo "== SE RESUELVEN EN RUNTIME (se espera que FALTEN) =="
for s in get_user_pages_remote get_task_mm mmput find_get_pid get_pid_task \
         put_pid put_task_struct d_path flush_dcache_page __put_page \
         vmem_map kallsyms_lookup_name; do
  if has "$s"; then echo "  exportado  $s  (aun así se usa runtime)"; else echo "  no exportado $s  (ok, runtime)"; fi
done
