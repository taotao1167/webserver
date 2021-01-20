#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __TT_WEB_H__
#include "tt_web.h"
#endif
#ifndef __TT_SHA1_H__
#include "tt_sha1.h"
#endif
#ifndef __MAVIEW_LICENSE_H__
#include "maview_license.h"
#endif
#ifndef __SQLITE3_ADAPTER_H__
#include "sqlite3_adapter.h"
#endif

#ifdef WATCH_RAM
#include "tt_malloc_debug.h"
#define MY_MALLOC(x) my_malloc((x), __func__, __LINE__)
#define MY_FREE(x) my_free((x), __func__, __LINE__)
#define MY_REALLOC(x, y) my_realloc((x), (y), __func__, __LINE__)
#else
#define MY_MALLOC(x) malloc((x))
#define MY_FREE(x) free((x))
#define MY_REALLOC(x, y) realloc((x), (y))
#endif

sqlite3 *g_db = NULL;

int init_database() {
	int ret = 0;
	RECORD_LIST *p_list = NULL;

	g_db = sql_open("license.db");
	if (g_db == NULL) {
		printf("Open database failed.\n");
		goto err_exit;
	}
	p_list = sql_query(g_db, "select * from sqlite_master where type = 'table' and name = 'license'");
	if (p_list) {
		if (p_list->head == NULL) {
			ret = sql_exec(g_db, "create table license("\
					"id integer primary key autoincrement, "\
					"version char(16), "\
					"issuer char(64), "\
					"subject char(64), "\
					"type char(16), "\
					"machine_id char(65), "\
					"create_tm int, "\
					"expire_tm int, "\
					"hash_algorithm char(16), "\
					"sign_algorithm char(16), "\
					"license_path text);");
			if (ret != 0) {
				printf("create table license failed.\n");
				goto err_exit;
			}
		}
		destroy_record_list(&p_list);
	} else {
		printf("select table license failed.\n");
		goto err_exit;
	}
	return 0;
err_exit:
	if (g_db) {
		sql_close(g_db);
		g_db = NULL;
	}
	if (p_list) {
		destroy_record_list(&p_list);
	}
	return -1;
}

int http_cgi_gen_license(HTTP_FD *p_link) {
	const char *retCode = "null";
	const char *version = "1.0", *issuer = "Kyland";
	const char *subject = NULL, *type = NULL, *machine_id = NULL, *err_code = ERROR_NO;;
	int days = 0;
	unsigned char *content = NULL;
	int content_len = 0;
	char sha1_output[41];
	char saved_path[256];
	FILE *fp = NULL;
	MAVIEW_LICENSE *license = NULL;

	if (0 == strcmp(p_link->method, "POST")) {
		retCode = "success";
		subject = web_post_str(p_link, "subject", "anonymous");
		type = web_post_str(p_link, "type", "trial");
		machine_id = web_post_str(p_link, "machine_id", NULL);
		days = atoi(web_post_str(p_link, "days", "30"));
		if (machine_id != NULL) {
			gen_license(&content, &content_len, "privatekey.pem", version, issuer, subject, type, machine_id, days);
			tt_sha1_hex(content, content_len, sha1_output);
			sprintf(saved_path, "download/%s", sha1_output);
			fp = fopen(saved_path, "wb");
			if (fp != NULL) {
				fwrite(content, content_len, 1, fp);
				fclose(fp);
			}
			license = read_license((char *)content, &err_code);
			sql_exec(g_db, "insert into license (version, issuer, subject, type, machine_id, create_tm, expire_tm, hash_algorithm, sign_algorithm, license_path) "\
					"values (\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", %s, %s, \"%s\", \"%s\", \"%s\")", \
					license->version, license->issuer, license->subject, license->license_type, license->machine_id, license->create, license->expires, license->hash_algorithm, license->sign_algorithm, saved_path);
			destroy_license(&license);
		} else {
			retCode = "failed";
		}
		web_printf(p_link, "{\"retCode\":[\"%s\"], \"path\":\"/%s\"}", retCode, saved_path);
		web_fin(p_link, 200);
	} else {
		web_fin(p_link, 403);
	}
	return 0;
}
int http_cgi_license_list(HTTP_FD *p_link) {
	const char *retCode = "null";
	int isfirst = 1;
	RECORD_LIST *p_list = NULL;
	RECORD *p_rec = NULL;
	int pageSize = 20, total = 0, maxPage = 0, curPage = 0;

	p_list = sql_query(g_db, "select count(*) as total from license");
	if (p_list) {
		if ((p_rec = p_list->head) != NULL) {
			total = atoi(record_get_str(p_rec, "total", "0"));
		}
		destroy_record_list(&p_list);
	}
	curPage = atoi(web_query_str(p_link, "curPage", "1"));
	if (curPage < 1) {
		curPage = 1;
	}
	if (total < 1) {
		maxPage = 1;
	} else {
		maxPage = (total - 1) / pageSize + 1;
	}
	if (curPage > maxPage) {
		curPage = maxPage;
	}

	web_printf(p_link, "{\"retCode\":[\"%s\"], \"total\":%d, \"curPage\":%d, \"pageSize\":%d, \"licenses\":[", \
		retCode, total, curPage, pageSize);
	p_list = sql_query(g_db, "select * from license limit %d, %d", pageSize * (curPage - 1), pageSize);
	if (p_list) {
		if (p_list->head) {
			for (p_rec = p_list->head; p_rec != NULL; p_rec = p_rec->next) {
				if (isfirst) {
					isfirst = 0;
				} else {
					web_printf(p_link, ",");
				}
				web_printf(p_link, "{\"id\":%s, \"version\":\"%s\", \"issuer\":\"%s\", \"subject\":\"%s\", \"licenseType\":\"%s\", "\
					"\"machineId\":\"%s\", \"create\":%s, \"expires\":%s, \"hashAlgorithm\":\"%s\",\"signAlgorithm\":\"%s\", \"path\":\"%s\"}", 
					record_get_str(p_rec, "id", "null"), record_get_str(p_rec, "version", "null"), 
					record_get_str(p_rec, "issuer", "null"), record_get_str(p_rec, "subject", "null"), 
					record_get_str(p_rec, "type", "null"), record_get_str(p_rec, "machine_id", "null"), 
					record_get_str(p_rec, "create_tm", "null"), record_get_str(p_rec, "expire_tm", "null"), 
					record_get_str(p_rec, "hash_algorithm", "null"), record_get_str(p_rec, "sign_algorithm", "null"), 
					record_get_str(p_rec, "license_path", "null"));
			}
		}
		destroy_record_list(&p_list);
	}
	web_printf(p_link, "]}");
	web_fin(p_link, 200);
	return 0;
}
int http_download_cert(HTTP_FD *p_link) {
	FILE *fp = NULL;
	size_t fsize = 0;
	unsigned char *fcontent = NULL;
	fp = fopen(p_link->path + 1, "rb");
	if (fp == NULL) {
		web_fin(p_link, 404);
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (fsize > 1024 * 1024) {
		web_fin(p_link, 500);
		return 0;
	}
	fcontent = (unsigned char *)MY_MALLOC(fsize + 1);
	if (fcontent == NULL) {
		web_fin(p_link, 500);
		return 0;
	}
	fread(fcontent, fsize, 1, fp);
	fclose(fp);
	fp = NULL;
	*(fcontent + fsize) = '\0';
	web_no_copy(p_link, fcontent, fsize, fsize, 1);
	web_set_header(p_link, "Content-Type", "application/octet-stream");
	web_set_header(p_link, "Content-Disposition", "attachment; filename=\"license\"");
	web_fin(p_link, 200);
	return 0;
}