/*
 * phmeop — driver de dispositivo de caracteres /dev/phmeop.
 *
 * Expone a un cliente con privilegios (root) cuatro operaciones de ioctl:
 *   OP_READ_MEM          0x801  leer memoria de otro proceso
 *   OP_WRITE_MEM         0x802  escribir memoria de otro proceso
 *   OP_MODULE_BASE       0x803  buscar la base de un módulo (.so) de otro proceso
 *   OP_MODULE_DATA_RANGE 0x804  rango escribible (.data/.bss) de un módulo
 *
 * Contrato (debe coincidir byte a byte con el cliente, ver phmeop.h):
 *   struct CopyMemory  { pid_t pid; uintptr_t addr; void *buffer; size_t size; };
 *   struct ModuleBase  { pid_t pid; char *name; uintptr_t base; };
 *   struct ModuleRange { pid_t pid; char *name; uintptr_t start; uintptr_t end; };
 *
 * Lectura/escritura PARCIAL: si una página intermedia no está mapeada, la
 * operación devuelve el número de bytes copiados hasta ese punto (en vez de
 * perder todo el trabajo). Solo devuelve un valor negativo si no copió nada.
 *
 * Reglas:
 *   - Solo un cliente a la vez (un segundo open() devuelve -EBUSY).
 *   - Abrir como root (open() devuelve -EPERM si no).
 *   - El ioctl debe hacerse desde código nativo (C/C++/JNI).
 *
 * Compatibilidad: se adapta a las firmas de get_user_pages_remote()
 * anteriores y posteriores a 4.9/4.11, y al renombrado de mmap_sem en 5.8.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/pid.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/capability.h>
#include <linux/atomic.h>
#include <asm/cacheflush.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#include <linux/mmap_lock.h>
#define phmeop_mmap_lock_read(mm)   mmap_read_lock(mm)
#define phmeop_mmap_lock_unlock(mm) mmap_read_unlock(mm)
#else
#include <linux/rwsem.h>
#define phmeop_mmap_lock_read(mm)   down_read(&(mm)->mmap_sem)
#define phmeop_mmap_lock_unlock(mm) up_read(&(mm)->mmap_sem)
#endif

#include "phmeop.h"

/* Solo un cliente a la vez. */
static atomic_t phmeop_busy = ATOMIC_INIT(0);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Devuelve la task del pid con su referencia tomada, o NULL. */
static struct task_struct *phmeop_get_task(pid_t pid)
{
    struct pid *pid_struct = find_get_pid(pid);
    struct task_struct *task = NULL;

    if (pid_struct) {
        task = get_pid_task(pid_struct, PIDTYPE_PID);
        put_pid(pid_struct);
    }
    return task; /* si != NULL, hay que put_task_struct() */
}

/*
 * Fija una página del espacio de direcciones del proceso objetivo en `addr`
 * y la devuelve en *out. `write` indica si se necesita acceso de escritura.
 */
static int phmeop_get_page(struct task_struct *task, struct mm_struct *mm,
                           unsigned long addr, bool write, struct page **out)
{
    long ret;
    unsigned int gup_flags = FOLL_FORCE | (write ? FOLL_WRITE : 0);

    *out = NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    /* 4.11+: sin parámetro task_struct */
    ret = get_user_pages_remote(mm, addr, 1, gup_flags, out, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    /* 4.9-4.10: gup_flags + task_struct */
    ret = get_user_pages_remote(task, mm, addr, 1, gup_flags, out, NULL, NULL);
#else
    /* < 4.9: write/force como ints + task_struct */
    ret = get_user_pages_remote(task, mm, addr, 1, write ? 1 : 0, 1, out, NULL);
#endif

    if (ret != 1) {
        if (ret > 0 && *out)
            put_page(*out);
        *out = NULL;
        return -EFAULT;
    }
    return 0;
}

/*
 * get_user_pages devuelve la CABECERA del compound page (THP de 2 MB). Para
 * tocar la dirección exacta hay que seleccionar el sub-page correcto antes de
 * kmap. Sin esto, en zonas THP siempre se lee/escribe el primer 4 KB y el
 * resto devuelve memoria equivocada.
 */
static struct page *phmeop_subpage(struct page *page, unsigned long addr)
{
    if (PageCompound(page)) {
        const unsigned long order = compound_order(page);
        if (order) {
            const unsigned long index =
                (addr >> PAGE_SHIFT) & ((1UL << order) - 1);
            return page + index;
        }
    }
    return page;
}

/* ------------------------------------------------------------------ */
/* Operaciones                                                         */
/* ------------------------------------------------------------------ */

static long phmeop_read_mem(pid_t pid, uintptr_t addr,
                            void __user *buffer, size_t size)
{
    struct task_struct *task;
    struct mm_struct *mm;
    unsigned long vaddr = (unsigned long)addr;
    size_t left = size;
    long ret = 0;

    if (!buffer || size == 0)
        return -EINVAL;

    task = phmeop_get_task(pid);
    if (!task)
        return -ESRCH;

    mm = get_task_mm(task);
    if (!mm) {
        put_task_struct(task);
        return -EINVAL;
    }

    while (left) {
        unsigned long off = vaddr & ~PAGE_MASK;
        size_t chunk = min_t(size_t, PAGE_SIZE - off, left);
        struct page *page;
        struct page *sub;
        void *kv;

        if (phmeop_get_page(task, mm, vaddr & PAGE_MASK, false, &page)) {
            ret = -EFAULT;
            break;
        }

        sub = phmeop_subpage(page, vaddr);
        kv = kmap(sub);
        if (copy_to_user(buffer, kv + off, chunk))
            ret = -EFAULT;
        kunmap(sub);
        put_page(page);

        if (ret)
            break;

        buffer += chunk;
        vaddr += chunk;
        left -= chunk;
    }

    mmput(mm);
    put_task_struct(task);
    if (ret)
        return left == size ? ret : (long)(size - left); /* parcial */
    return (long)size;
}

static long phmeop_write_mem(pid_t pid, uintptr_t addr,
                             const void __user *buffer, size_t size)
{
    struct task_struct *task;
    struct mm_struct *mm;
    unsigned long vaddr = (unsigned long)addr;
    size_t left = size;
    long ret = 0;

    if (!buffer || size == 0)
        return -EINVAL;

    task = phmeop_get_task(pid);
    if (!task)
        return -ESRCH;

    mm = get_task_mm(task);
    if (!mm) {
        put_task_struct(task);
        return -EINVAL;
    }

    while (left) {
        unsigned long off = vaddr & ~PAGE_MASK;
        size_t chunk = min_t(size_t, PAGE_SIZE - off, left);
        struct page *page;
        struct page *sub;
        void *kv;

        if (phmeop_get_page(task, mm, vaddr & PAGE_MASK, true, &page)) {
            ret = -EFAULT;
            break;
        }

        sub = phmeop_subpage(page, vaddr);
        kv = kmap(sub);
        if (copy_from_user(kv + off, buffer, chunk))
            ret = -EFAULT;
        else
            flush_dcache_page(sub);
        kunmap(sub);
        put_page(page);

        if (ret)
            break;

        buffer += chunk;
        vaddr += chunk;
        left -= chunk;
    }

    mmput(mm);
    put_task_struct(task);
    if (ret)
        return left == size ? ret : (long)(size - left); /* parcial */
    return (long)size;
}

/* Copia el nombre del módulo desde el llamador con límite fijo.
 * El struct no trae longitud, así que el máximo lo impone el driver. */
static long phmeop_copy_name(const char __user *name, char *out, size_t out_size)
{
    long len;

    if (!name || !out || out_size < 2)
        return -EINVAL;

    len = strncpy_from_user(out, name, out_size - 1);
    if (len < 0)
        return len;                 /* -EFAULT */
    out[len] = '\0';
    return len == 0 ? -EINVAL : len;
}

static long phmeop_module_base(pid_t pid, const char __user *name,
                               uintptr_t *out)
{
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    char namebuf[256];
    char pathbuf[PATH_MAX];
    long len;

    if (!out)
        return -EINVAL;

    len = phmeop_copy_name(name, namebuf, sizeof(namebuf));
    if (len < 0)
        return len;

    task = phmeop_get_task(pid);
    if (!task)
        return -ESRCH;

    mm = get_task_mm(task);
    if (!mm) {
        put_task_struct(task);
        return -EINVAL;
    }

    phmeop_mmap_lock_read(mm);
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        char *path;

        if (!vma->vm_file)
            continue;

        path = d_path(&vma->vm_file->f_path, pathbuf, sizeof(pathbuf));
        if (IS_ERR(path))
            continue;

        /* Coincide si la ruta del mapping contiene el nombre pedido
         * (p. ej. "libgame.so"). La primera coincidencia es la base. */
        if (strstr(path, namebuf)) {
            *out = vma->vm_start;
            phmeop_mmap_lock_unlock(mm);
            mmput(mm);
            put_task_struct(task);
            return 0;
        }
    }
    phmeop_mmap_lock_unlock(mm);

    mmput(mm);
    put_task_struct(task);
    return -ENOENT;
}

/*
 * Unión de los mappings escribibles del módulo (rango [start, end) de la
 * sección .data/.bss). El cliente usa esto para escanear variables globales
 * sin leer /proc/<pid>/maps (que puede estar restringido por SELinux).
 */
static long phmeop_module_data_range(pid_t pid, const char __user *name,
                                     uintptr_t *start, uintptr_t *end)
{
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    char namebuf[256];
    char pathbuf[PATH_MAX];
    unsigned long lo = 0, hi = 0;
    bool found = false;
    long len;

    if (!start || !end)
        return -EINVAL;

    len = phmeop_copy_name(name, namebuf, sizeof(namebuf));
    if (len < 0)
        return len;

    task = phmeop_get_task(pid);
    if (!task)
        return -ESRCH;

    mm = get_task_mm(task);
    if (!mm) {
        put_task_struct(task);
        return -EINVAL;
    }

    phmeop_mmap_lock_read(mm);
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        char *path;

        if (!vma->vm_file || !(vma->vm_flags & VM_WRITE))
            continue;

        path = d_path(&vma->vm_file->f_path, pathbuf, sizeof(pathbuf));
        if (IS_ERR(path) || !strstr(path, namebuf))
            continue;

        if (!found) {
            lo = vma->vm_start;
            hi = vma->vm_end;
            found = true;
        } else {
            lo = min(lo, (unsigned long)vma->vm_start);
            hi = max(hi, (unsigned long)vma->vm_end);
        }
    }
    phmeop_mmap_lock_unlock(mm);

    mmput(mm);
    put_task_struct(task);

    if (!found)
        return -ENOENT;

    *start = lo;
    *end = hi;
    return 0;
}

/* ------------------------------------------------------------------ */
/* fops                                                                */
/* ------------------------------------------------------------------ */

static int phmeop_open(struct inode *inode, struct file *file)
{
    /* Abrir solo como root. */
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    /* Un único cliente a la vez. */
    if (atomic_cmpxchg(&phmeop_busy, 0, 1))
        return -EBUSY;

    return 0;
}

static int phmeop_release(struct inode *inode, struct file *file)
{
    atomic_set(&phmeop_busy, 0);
    return 0;
}

static long phmeop_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case OP_READ_MEM: {
        struct CopyMemory cm;
        if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
            return -EFAULT;
        return phmeop_read_mem(cm.pid, cm.addr,
                               (void __user *)cm.buffer, cm.size);
    }
    case OP_WRITE_MEM: {
        struct CopyMemory cm;
        if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
            return -EFAULT;
        return phmeop_write_mem(cm.pid, cm.addr,
                                (const void __user *)cm.buffer, cm.size);
    }
    case OP_MODULE_BASE: {
        struct ModuleBase mb;
        uintptr_t base = 0;
        long ret;

        if (copy_from_user(&mb, (void __user *)arg, sizeof(mb)))
            return -EFAULT;

        ret = phmeop_module_base(mb.pid, (const char __user *)mb.name, &base);
        if (ret == 0) {
            if (copy_to_user(&((struct ModuleBase __user *)arg)->base,
                             &base, sizeof(base)))
                return -EFAULT;
        }
        return ret;
    }
    case OP_MODULE_DATA_RANGE: {
        struct ModuleRange mr;
        uintptr_t start = 0, end = 0;
        long ret;

        if (copy_from_user(&mr, (void __user *)arg, sizeof(mr)))
            return -EFAULT;

        ret = phmeop_module_data_range(mr.pid, (const char __user *)mr.name,
                                       &start, &end);
        if (ret == 0) {
            struct ModuleRange __user *out = (struct ModuleRange __user *)arg;
            if (copy_to_user(&out->start, &start, sizeof(start)) ||
                copy_to_user(&out->end, &end, sizeof(end)))
                return -EFAULT;
        }
        return ret;
    }
    default:
        return -ENOTTY;
    }
}

static const struct file_operations phmeop_fops = {
    .owner          = THIS_MODULE,
    .open           = phmeop_open,
    .release        = phmeop_release,
    .unlocked_ioctl = phmeop_ioctl,
    .llseek         = no_llseek,
};

static struct miscdevice phmeop_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "phmeop",
    .fops  = &phmeop_fops,
    .mode  = 0666,
};

/* ------------------------------------------------------------------ */
/* init / exit                                                         */
/* ------------------------------------------------------------------ */

static int __init phmeop_init(void)
{
    int ret = misc_register(&phmeop_misc);
    if (ret)
        pr_err("phmeop: misc_register falló (%d)\n", ret);
    else
        pr_info("phmeop: /dev/phmeop registrado\n");
    return ret;
}

static void __exit phmeop_exit(void)
{
    misc_deregister(&phmeop_misc);
    pr_info("phmeop: descargado\n");
}

module_init(phmeop_init);
module_exit(phmeop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dark Code");
MODULE_DESCRIPTION("phmeop: acceso a memoria de procesos vía /dev/phmeop");
