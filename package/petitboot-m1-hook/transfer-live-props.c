/*
 * This program transfers some properties from the current DT
 * into the DT being prepared for the next-to-be-loaded kernel.
 *
 * The properties in question are not static and were filled in
 * by a bootloader that came before us in the boot chain.
 *
 * This program is derived from utils/hooks/30-dtb-updates.c
 * of petitboot, and is distributed under the terms of
 * the GNU General Public License v2, see COPYING in petitboot
 * source. NO WARRANTY ATTACHED.
 */

#define _GNU_SOURCE

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#include <libfdt.h>

#define EXTRA_SPACE	1024

/*
 * read_file(), write_fd(), replace_file()
 *
 * copied from petitboot's lib/file/file.c
 *
 * Copyright (C) 2013 Jeremy Kerr <jk@ozlabs.org>
 * Copyright (C) 2016 Raptor Engineering, LLC
 */ 
static const int max_file_size = 1024 * 1024;
int read_file(void *ctx, const char *filename, char **bufp, int *lenp)
{
	struct stat statbuf;
	int rc, fd, i, len;
	char *buf;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -1;

	rc = fstat(fd, &statbuf);
	if (rc < 0)
		goto err_close;

	len = statbuf.st_size;
	if (len > max_file_size)
		goto err_close;

	buf = (char *) malloc(len + 1);
	if (!buf)
		goto err_close;

	for (i = 0; i < len; i += rc) {
		rc = read(fd, buf + i, len - i);

		/* unexpected EOF: trim and return */
		if (rc == 0) {
			len = i;
			break;
		}

		if (rc < 0)
			goto err_free;

	}

	buf[len] = '\0';

	close(fd);
	*bufp = buf;
	*lenp = len;
	return 0;

err_free:
	free(buf);
err_close:
	close(fd);
	return -1;
}

static int write_fd(int fd, char *buf, int len)
{
	int i, rc;

	for (i = 0; i < len; i += rc) {
		rc = write(fd, buf + i, len - i);
		if (rc < 0 && errno != -EINTR)
			return rc;
	}

	return 0;
}

int replace_file(const char *filename, char *buf, int len)
{
	char *tempfile;
	mode_t oldmask;
	int rc, fd;


	tempfile = malloc(strlen(filename) + strlen(".XXXXXX") + 1);
	sprintf(tempfile, "%s.XXXXXX", filename);

	oldmask = umask(0644);
	fd = mkstemp(tempfile);
	umask(oldmask);
	if (fd < 0) {
		free(tempfile);
		return fd;
	}

	rc = write_fd(fd, buf, len);
	if (rc) {
		unlink(tempfile);
	} else {
		rc = rename(tempfile, filename);
	}

	free(tempfile);

	fchmod(fd, 0644);

	close(fd);
	return rc;
}

struct offb_ctx {
	const char	*dtb_name;
	void		*dtb;
	int			dtb_node;
	const char	*path;

	const char	*boot_dtb_name;
	void		*boot_dtb;
};

static int load_dtb(struct offb_ctx *ctx)
{
	char *buf;
	int len;
	int rc;

	rc = read_file(ctx, ctx->dtb_name, &buf, &len);
	if (rc) {
		warn("Error reading %s", ctx->dtb_name);
		return rc;
	}

	rc = fdt_check_header(buf);
	if (rc || (int)fdt_totalsize(buf) > len) {
		warnx("invalid dtb: %s (rc %d)", ctx->dtb_name, rc);
		return -1;
	}

	len = fdt_totalsize(buf) + EXTRA_SPACE;

	ctx->dtb = (char *) malloc(len);
	if (!ctx->dtb) {
		warn("Failed to allocate space for dtb\n");
		return -1;
	}
	fdt_open_into(buf, ctx->dtb, len);

	return 0;
}

static int load_boot_dtb(struct offb_ctx *ctx)
{	
	char *buf;
	int len;
	int rc;

	rc = read_file(ctx, ctx->boot_dtb_name, &buf, &len);
	if (rc) {
		warn("Error reading %s", ctx->boot_dtb_name);
		return rc;
	}

	rc = fdt_check_header(buf);
	if (rc || (int)fdt_totalsize(buf) > len) {
		warnx("Invalid dtb: %s (rc %d)", ctx->boot_dtb_name, rc);
		return -1;
	}

	len = fdt_totalsize(buf);

	ctx->boot_dtb = malloc(len);
	if (!ctx->dtb) {
		warn("Failed to allocate space for dtb\n");
		return -1;
	}
	fdt_open_into(buf, ctx->boot_dtb, len);

	return 0;
}

int walk_to_subnode(const void *fdt, int parentoffset, const char *path)
{
	const char *p, *n;
	int offset = parentoffset;

	for (p = path;; p = n+1) {
		n = strchrnul(p, '/');
		offset = fdt_subnode_offset_namelen(fdt, offset, p, n - p);
		if (offset < 0 || *n == '\0') 
			break;
	}

	return offset;
}

static void copy_prop(struct offb_ctx *ctx, char *path, char *propname)
{
	int node, boot_node, len, err;
	const void *prop;

	node = walk_to_subnode(ctx->dtb, 0, path);
	boot_node = walk_to_subnode(ctx->boot_dtb, 0, path);

	if (node < 0) {
		warnx("Node %s not found in user-provided DT", path);
		return;
	}

	if (boot_node < 0) {
		warnx("Node %s not found in boot DT", path);
		return;
	}

	prop = fdt_getprop(ctx->boot_dtb, boot_node, propname, &len);

	if (prop == 0) {
		warnx("fdt_getprop: %s/%s: error %d", path, propname, len);
		return;
	}

	err = fdt_setprop(ctx->dtb, node, propname, prop, len);

	if (err < 0) {
		warnx("fdt_setprop: %s/%s: error %d", path, propname, err);
	}
}

static void copy_disabled_state(struct offb_ctx *ctx, char *path)
{
	int node, boot_node, len, err;
	const void *prop;

	node = walk_to_subnode(ctx->dtb, 0, path);
	boot_node = walk_to_subnode(ctx->boot_dtb, 0, path);

	if (node < 0) {
		warnx("Node %s not found in user-provided DT", path);
		return;
	}

	if (boot_node < 0) {
		warnx("Node %s not found in boot DT", path);
		return;
	}

	prop = fdt_getprop(ctx->boot_dtb, boot_node, "status", &len);
	if (prop == 0 && len != -FDT_ERR_NOTFOUND) {
		warnx("fdt_getprop: %s/status: error %d", path, len);
		return;
	}

	if (len == -FDT_ERR_NOTFOUND || (strncmp(prop, "okay", len) == 0)) {
		/* node is enabled */
		fdt_delprop(ctx->dtb, node, "status");
	} else {
		err = fdt_setprop_string(ctx->dtb, node, "status", "disabled");
		if (err < 0)
			warnx("fdt_setprop: %s/status: error %d", path, err);
	}
}

static int write_devicetree(struct offb_ctx *ctx)
{
	int rc;

	fdt_pack(ctx->dtb);

	rc = replace_file(ctx->dtb_name, ctx->dtb, fdt_totalsize(ctx->dtb));
	if (rc)
		warn("Failed to write file %s", ctx->dtb_name);

	return rc;
}

static int wipe_kaslr_seed(struct offb_ctx *ctx)
{
	int chosen;
	chosen = fdt_subnode_offset(ctx->dtb, 0, "chosen");
	if (chosen <= 0) {
		warnx("Failed to find chosen\n");
		return -1;
	}
	uint64_t zero = 0;
	return fdt_setprop(ctx->dtb, chosen, "kaslr-seed", &zero, sizeof(zero));
}

static void transfer_resmems(struct offb_ctx *ctx)
{
	uint64_t addr, size;
	int err;
	int nresmems;

	// first remove the resmems already present
	nresmems = fdt_num_mem_rsv(ctx->dtb);
	for (int i = nresmems; i > 0; i--)
		fdt_del_mem_rsv(ctx->dtb, i-1);

	// copy new resmems in
	nresmems = fdt_num_mem_rsv(ctx->boot_dtb);
	for (int i = 0; i < nresmems; i++) {
		err = fdt_get_mem_rsv(ctx->boot_dtb, i, &addr, &size);
		if (err < 0) {
			warnx("fdt_get_mem_rsv: error %d", err);
			continue;
		}
		fdt_add_mem_rsv(ctx->dtb, addr, size);
	}
}

int main(void)
{
	struct offb_ctx *ctx;
	int rc = 0;

	ctx = calloc(1, sizeof(struct offb_ctx));

	ctx->dtb_name = getenv("boot_dtb");
	if (!ctx->dtb_name) {
		free(ctx);
		return EXIT_SUCCESS;
	}

	ctx->boot_dtb_name = getenv("model_dtb");
	if (!ctx->boot_dtb_name)
		ctx->boot_dtb_name = "/sys/firmware/fdt";

	if (load_dtb(ctx)) {
		warnx("Failed to load dtb");
		free(ctx);
		return 1;
	}

	if (load_boot_dtb(ctx)) {
		warnx("Failed to load boot dtb");
		free(ctx);
		return 1;
	}

	copy_prop(ctx, "cpus/cpu@0", "cpu-release-addr");
	copy_prop(ctx, "cpus/cpu@1", "cpu-release-addr");
	copy_prop(ctx, "cpus/cpu@2", "cpu-release-addr");
	copy_prop(ctx, "cpus/cpu@3", "cpu-release-addr");
	copy_prop(ctx, "cpus/cpu@10100", "cpu-release-addr");
	copy_prop(ctx, "cpus/cpu@10101", "cpu-release-addr");
	copy_prop(ctx, "cpus/cpu@10102", "cpu-release-addr");
	copy_prop(ctx, "cpus/cpu@10103", "cpu-release-addr");

	copy_disabled_state(ctx, "cpus/cpu@0");
	copy_disabled_state(ctx, "cpus/cpu@1");
	copy_disabled_state(ctx, "cpus/cpu@2");
	copy_disabled_state(ctx, "cpus/cpu@3");
	copy_disabled_state(ctx, "cpus/cpu@10100");
	copy_disabled_state(ctx, "cpus/cpu@10101");
	copy_disabled_state(ctx, "cpus/cpu@10102");
	copy_disabled_state(ctx, "cpus/cpu@10103");

	copy_prop(ctx, "chosen/framebuffer", "reg");
	copy_prop(ctx, "chosen/framebuffer", "format");
	copy_prop(ctx, "chosen/framebuffer", "stride");
	copy_prop(ctx, "chosen/framebuffer", "height");
	copy_prop(ctx, "chosen/framebuffer", "width");

	copy_disabled_state(ctx, "chosen/framebuffer");

	copy_prop(ctx, "memory", "reg");

	transfer_resmems(ctx);

	// wipe KASLR seed to make kexec-tools happy
	if (wipe_kaslr_seed(ctx)) {
		warnx("Failed to wipe /chosen/kaslr-seed\n");
		rc = -1;
	}

	if (write_devicetree(ctx)) {
		warnx("Failed to write back device tree\n");
		rc = -1;
	}

	free(ctx);
	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
