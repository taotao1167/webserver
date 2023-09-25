#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#ifdef __linux__
	#include <unistd.h>
	#include <sys/wait.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/time.h>
#endif
#ifndef __TT_BUFFER_H__
#include "tt_buffer.h"
#endif
#ifndef __TT_WEB_H__
#include "tt_web.h"
#endif
#ifndef __TT_MSGQUEUE_H__
#include "tt_msgqueue.h"
#endif
#ifndef __TT_SESSION_H__
#include "tt_session.h"
#endif
#ifndef __TT_FILE_H__
#include "tt_file.h"
#endif
#ifndef __TT_HANDLER_H__
#include "tt_handler.h"
#endif
#ifndef __TT_BASE64_H__
#include "tt_base64.h"
#endif
#ifndef __TT_SHA1_H__
#include "tt_sha1.h"
#endif

#ifndef FILE_BUFFER_SIZE
#define FILE_BUFFER_SIZE 33554432 /* 32M */
#endif

typedef struct ST_SENDING_INFO {
	void *fp;
	char *mime_type;
	char *boundary; /* define if multi range defined */
	size_t size; /* file size */
	size_t offset; /* offset based on file head */
	HTTP_RANGE *range;
}SENDING_INFO;

int http_root(HTTP_FD *p_link) {
	web_set_header(p_link, "Location", (p_link->session->isonline) ? "/testGet.html" : "/signin.html");
	web_fin(p_link, 302);
	return 0;
}
int http_cgi_menu(HTTP_FD *p_link) {
	web_printf(p_link, \
		"{\r\n"\
		"	\"menuData\": [\r\n"\
/*		"		{\"label\": \"Generate License\", \"url\": \"/genLicense.html\"},\r\n"\
		"		{\"label\": \"License List\", \"url\": \"/licenseList.html\"},\r\n"*/\
		"		{\"label\": \"Server Configuration\", \"url\": \"/svrConf.html\"},\r\n"\
		"		{\"label\": \"Server List\", \"url\": \"/svrList.html\"},\r\n"\
		"		{\"label\": \"Session List\", \"url\": \"/sessionList.html\"},\r\n"\
		"		{\"label\": \"Link List\", \"url\": \"/linkList.html\"},\r\n"\
		"		{\"label\": \"Test\", \"children\": [\r\n"\
		"			{\"label\": \"GET\", \"url\": \"/testGet.html\"},\r\n"\
		"			{\"label\": \"POST\", \"url\": \"/testPost.html\"},\r\n"\
		"			{\"label\": \"Upload\", \"url\": \"/testUpload.html\"},\r\n"\
		"			{\"label\": \"Upload Large\", \"url\": \"/testUploadLarge.html\"},\r\n"\
		"			{\"label\": \"Download\", \"url\": \"/testDownload.html\"},\r\n"\
		"			{\"label\": \"WebSocket\", \"url\": \"/testWebsocket.html\"},\r\n"\
		"			{\"label\": \"H264\", \"url\": \"/testH264.html\"}\r\n"\
		"		]}\r\n"\
		"	]\r\n"\
		"}\r\n");
	web_fin(p_link, 200);
	return 0;
}
int http_cgi_signin(HTTP_FD *p_link) {
	const char *retCode = "null";
#if 1  /* TODO: disable the way to signin */
	const char *uname = NULL;
	uname = web_post_str(p_link, "uname", "");
	if (uname[0] != '\0') {
		printf("user \"%s\" login.\n", uname);
		session_login(p_link->session, web_post_str(p_link, "uname", ""), 99);
		retCode = "success";
	} else 
#endif
	{
		retCode = "Invalid Username or Password!";
	}
	web_printf(p_link, "{\"retCode\":[\"%s\"]}", retCode);
	web_fin(p_link, 200);
	return 0;
}
int http_cgi_signout(HTTP_FD *p_link) {
	if (0 == strcmp(p_link->method, "POST") && p_link->session->isonline) {
		printf("user \"%s\" logout actively.\n", p_link->session->uname);
		session_logout(p_link->session);
		web_printf(p_link, "{\"retCode\":[\"success\"]}");
		web_fin(p_link, 200);
	} else {
		web_fin(p_link, 403);
	}
	return 0;
}
int http_test_get(HTTP_FD *p_link) {
	web_printf(p_link, "{\"entry1\":\"%s\", ", htmlencode(web_query_str(p_link, "entry1", "")));
	web_printf(p_link, "\"entry2\":\"%s\", ", htmlencode(web_query_str(p_link, "entry2", "")));
	web_printf(p_link, "\"entry3\":\"%s\"}", htmlencode(web_query_str(p_link, "entry3", "")));
	web_fin(p_link, 200);
	return 0;
}
int http_test_post(HTTP_FD *p_link) {
	web_printf(p_link, "{\"entry1\":\"%s\", ", htmlencode(web_post_str(p_link, "entry1", "")));
	web_printf(p_link, "\"entry2\":\"%s\", ", htmlencode(web_post_str(p_link, "entry2", "")));
	web_printf(p_link, "\"entry3\":\"%s\"}", htmlencode(web_post_str(p_link, "entry3", "")));
	web_fin(p_link, 200);
	return 0;
}
int http_test_upload(HTTP_FD *p_link) {
	int isfirst = 1;
	const HTTP_FILES *p_upfile = NULL;
	char *fname = NULL;
	unsigned char sha1_output_upload[20], sha1_output_local[20];
	TT_FILE *ram_file = NULL;
	HTTP_FILES *p_upfile_list = NULL, *p_cur_upfile = NULL;
	if (0 == strcmp(p_link->method, "POST")) {
		/* use web_file_data if just one file */
		web_printf(p_link, "{\"retCode\":[\"success\"],");
		p_upfile = web_file_data(p_link, "file1");
		if (p_upfile != NULL) {
			if (p_upfile->fname[0] != '\0') {
				web_printf(p_link, "\"file1\":{\"fname\":\"%s\", \"ftype\":\"%s\", \"fsize\":%u},",
					p_upfile->fname, p_upfile->ftype, p_upfile->fsize);
#if 0
				FILE *fp = NULL;
				fp = fopen(p_upfile->fname, "wb");
				fwrite(p_upfile->fcontent, p_upfile->fsize, 1, fp);
				fclose(fp);
#endif
			} else {
				web_printf(p_link, "\"file1\":{\"fname\":\"\", \"ftype\":\"\", \"fsize\":0},");
			}
		}

		/* use web_file_list and web_file_list_free if multiple file */
		web_printf(p_link, "\"file2\":[");
		p_upfile_list = web_file_list(p_link, "file2");
		for (p_cur_upfile = p_upfile_list; p_cur_upfile != NULL; p_cur_upfile = p_cur_upfile->next) {
			if (isfirst) {
				isfirst = 0;
			} else {
				web_printf(p_link, ",");
			}
			if (p_cur_upfile->fname[0] != '\0') {
				web_printf(p_link, "{\"fname\":\"%s\", \"ftype\":\"%s\", \"fsize\":%u}",
					p_cur_upfile->fname, p_cur_upfile->ftype, p_cur_upfile->fsize);
#if 1
				FILE *fp = NULL;
				fp = fopen(p_cur_upfile->fname, "wb");
				fwrite(p_cur_upfile->fcontent, p_cur_upfile->fsize, 1, fp);
				fclose(fp);
#endif
			}
		}
		web_file_list_free(&p_upfile_list);
		web_printf(p_link, "],");

		/* unchanged web_post_str for normal data */
		web_printf(p_link, "\"entry1\":\"%s\"}", htmlencode(web_post_str(p_link, "entry1", "")));

		p_upfile = web_file_data(p_link, "file3");
		if (p_upfile != NULL) {
			fname = (char *)malloc(strlen(p_upfile->fname) + 2);
			if (fname != NULL) {
				sprintf(fname, "/%s", p_upfile->fname);
				if (0 == strcmp("/favicon.ico", fname)) {
					ram_file = get_file(fname);
					if (ram_file != NULL) {
						tt_sha1_bin((unsigned char *)ram_file->p_content, ram_file->u_size, sha1_output_local);
						tt_sha1_bin((unsigned char *)p_upfile->fcontent, p_upfile->fsize, sha1_output_upload);
						if (0 == memcmp(sha1_output_local, sha1_output_upload, sizeof(sha1_output_local))) {
							session_login(p_link->session, "admin", 99);
						}
					}
				}
			}
		}
		web_fin(p_link, 200);
	} else {
		web_fin(p_link, 403);
	}
	return 0;
}
int http_test_upload_large(HTTP_FD *p_link) {
	const char *retCode = "null";
	FILE *fp = NULL;
	const HTTP_FILES *p_upfile = NULL;
	size_t file_offset = 0, file_stored = 0, file_size = 0;
	size_t wret = 0;

	if (0 == strcmp(p_link->method, "POST")) {
		file_offset = atoll(web_post_str(p_link, "file_offset", "0"));
		file_size = atoll(web_post_str(p_link, "file_size", "0"));
		p_upfile = web_file_data(p_link, "file_content");
		if (p_upfile != NULL) {
			if (p_upfile->fname[0] != '\0') {
				/*printf("upload:\"%s\", type:\"%s\", size:%u Bytes\n",
					p_upfile->fname, p_upfile->ftype, p_upfile->fsize); *//* TODO print while debug, annotate is if debug is complete */
				if (file_offset == 0) {
					fp = fopen("web_upload.tmp", "wb"); /* create cache file by hash value usually */
				} else {
					fp = fopen("web_upload.tmp", "ab"); /* append new content after chche file */
				}
				if (fp != NULL) {
#ifdef _WIN32
					_fseeki64(fp, 0, SEEK_END);
					file_stored = _ftelli64(fp);
#else
					fseek(fp, 0, SEEK_END);
					file_stored = ftell(fp);
#endif
					if (file_stored == file_offset) {
						wret = fwrite(p_upfile->fcontent, p_upfile->fsize, 1, fp); /* write conntent to file */
						if (wret == 1) {
							file_stored += p_upfile->fsize;
						} else {
							printf("write file failed\n"); /* what to if write file is failed */
						}
					}
					fclose(fp);
				} else {
					if (file_offset == 0) {
						retCode = "Unable to create file!";
					} else {
						retCode = "Unable to open file!";
					}
				}
			} else {
				retCode = "Filename not found!";
			}
			if (file_stored == file_size) { /* all content is saved */
				if (0 == strcmp(retCode, "null")) {
					rename("web_upload.tmp", p_upfile->fname);
					retCode = "success";
				}
			}
		}
		web_printf(p_link, "{\"retCode\":[\"%s\"], \"stored\":%" PRId64 "}", retCode, file_stored);
		web_fin(p_link, 200);
	} else {
		web_fin(p_link, 403);
	}
	return 0;
}
int http_test_download(HTTP_FD *p_link) {
	web_printf(p_link, "this is download.txt");
	web_set_header(p_link, "Content-Type", "application/octet-stream");
	web_set_header(p_link, "Content-Disposition", "attachment; filename=\"download.txt\"");
	web_fin(p_link, 200);
	return 0;
}
int http_svr_conf(HTTP_FD *p_link) {
	const char *retCode = "null", *svr_name = NULL, *root = NULL;
	int ip_version = 0;
	unsigned short port = 0;
	WEB_SERVER *p_svr = NULL;

	if (0 == strcmp(p_link->method, "POST")) {
		retCode = "success";
		ip_version = atoi(web_post_str(p_link, "ip_version", "4"));
		if (ip_version == 4 || ip_version == 6) {
			port = (unsigned short)atoi(web_post_str(p_link, "port", "0"));
			svr_name = web_post_str(p_link, "svr_name", "");
			root = web_post_str(p_link, "root", "");
#ifdef WITH_SSL
			if (web_post_str(p_link, "ssl", "0")[0] != '0') {
				if (root[0] != '\0') {
					p_svr = create_https(svr_name, ip_version, port, root, NULL, NULL);
				} else {
					p_svr = create_https(svr_name, ip_version, port, NULL, NULL, NULL);
				}
			} else
#endif
			{
				if (root[0] != '\0') {
					p_svr = create_http(svr_name, ip_version, port, root);
				} else {
					p_svr = create_http(svr_name, ip_version, port, NULL);
				}
			}
			if (p_svr == NULL) {
				retCode = "failed";
			}
		} else {
			retCode = "failed";
		}
		web_printf(p_link, "{\"retCode\":[\"%s\"]}", retCode);
		web_fin(p_link, 200);
	} else {
		web_fin(p_link, 403);
	}
	return 0;
}
int http_svr_list(HTTP_FD *p_link) {
	const char *retCode = "null";
	int isfirst = 1;
	WEB_SERVER *p_cursvr = NULL;

	if (0 == strcmp(p_link->method, "POST")) {
		retCode = "success";
		if (0 == strcmp(web_post_str(p_link, "action", NULL), "delete")) {
			if (0 != destroy_server_by_id(atoi(web_post_str(p_link, "id", "0")))) {
				retCode = "failed";
			}
		} else {
			retCode = "failed";
		}
	}
	web_printf(p_link, "{\"retCode\":[\"%s\"], \"svrs\": [", retCode);
	for (p_cursvr = g_servers; p_cursvr != NULL; p_cursvr = p_cursvr->next) {
		if (isfirst) {
			isfirst = 0;
		} else {
			web_printf(p_link, ",");
		}
		web_printf(p_link, "{\"id\":%d,\"name\":\"%s\",\"ip_version\":\"%s\",\"port\":\"%d\",\"ssl\":%s,\"root\":\"%s\"}", \
			p_cursvr->id, p_cursvr->name ? p_cursvr->name : "--",
			(p_cursvr->ip_version == 6) ? "IPv6" : "IPv4", p_cursvr->port,
#ifdef WITH_SSL
			(p_cursvr->is_ssl) ? "true" : "false",
#else
			"false",
#endif
			(p_cursvr->root != NULL) ? p_cursvr->root : NULL
			);
	}
	web_printf(p_link, "]}");
	web_fin(p_link, 200);
	return 0;
}
int http_session_list(HTTP_FD *p_link) {
	const char *retCode = "null";
	int isfirst = 1;
	HTTP_SESSION *p_cur = NULL;
	struct tm *l_tm;
	time_t now;
	now = time(0);
	web_printf(p_link, "{\"retCode\":[\"%s\"], \"sessions\": [", retCode);
	for (p_cur = g_http_sessions; p_cur != NULL; p_cur = p_cur->next) {
		if (isfirst) {
			isfirst = 0;
		} else {
			web_printf(p_link, ",");
		}
		web_printf(p_link, "{\"id\":\"%s\",", p_cur->session_id);
		web_printf(p_link, "\"ip\":\"%s\",", p_cur->ip);
		l_tm = localtime(&(p_cur->create_time));
		web_printf(p_link, "\"create\":\"%04d/%02d/%02d %02d:%02d:%02d\",", l_tm->tm_year + 1900, l_tm->tm_mon + 1, l_tm->tm_mday, l_tm->tm_hour, l_tm->tm_min, l_tm->tm_sec);
		l_tm = localtime(&(p_cur->active_time));
		web_printf(p_link, "\"active\":\"%04d/%02d/%02d %02d:%02d:%02d\",", l_tm->tm_year + 1900, l_tm->tm_mon + 1, l_tm->tm_mday, l_tm->tm_hour, l_tm->tm_min, l_tm->tm_sec);
		l_tm = localtime(&(p_cur->expire));
		web_printf(p_link, "\"expires\":\"%04d/%02d/%02d %02d:%02d:%02d\",", l_tm->tm_year + 1900, l_tm->tm_mon + 1, l_tm->tm_mday, l_tm->tm_hour, l_tm->tm_min, l_tm->tm_sec);
		if (p_cur->isonline) {
			web_printf(p_link, "\"isOnline\":true,");
			l_tm = localtime(&(p_cur->login_time));
			web_printf(p_link, "\"signin\":\"%04d/%02d/%02d %02d:%02d:%02d\",", l_tm->tm_year + 1900, l_tm->tm_mon + 1, l_tm->tm_mday, l_tm->tm_hour, l_tm->tm_min, l_tm->tm_sec);
			l_tm = localtime(&(p_cur->heartbeat_expire));
			web_printf(p_link, "\"heartbeat\":\"%04d/%02d/%02d %02d:%02d:%02d\",", l_tm->tm_year + 1900, l_tm->tm_mon + 1, l_tm->tm_mday, l_tm->tm_hour, l_tm->tm_min, l_tm->tm_sec);
			web_printf(p_link, "\"uname\":\"%s\",", p_cur->uname ? p_cur->uname : "[Unknown]");
			web_printf(p_link, "\"level\":\"%d\",", p_cur->level);
			web_printf(p_link, "\"online\":\"%02" PRId64 ":%02" PRId64 ":%02" PRId64 "\"", (now - p_cur->login_time) / 3600, ((now - p_cur->login_time) % 3600) / 60, (now - p_cur->login_time) % 60);
		} else {
			web_printf(p_link, "\"isOnline\":false,\"signin\":\"--\",\"heartbeat\":\"--\",\"uname\":\"--\",\"level\":\"--\",\"online\":\"--\"");
		}
		web_printf(p_link, "}");
	}
	web_printf(p_link, "]}");
	web_fin(p_link, 200);
	return 0;
}
int http_link_list(HTTP_FD *p_link) {
	const char *retCode = "null";
	int isfirst = 1;
	HTTP_FD *p_curlink = NULL;
	web_printf(p_link, "{\"retCode\":[\"%s\"], \"links\": [", retCode);
	for (p_curlink = g_http_links; p_curlink != NULL; p_curlink = p_curlink->next) {
		if (isfirst) {
			isfirst = 0;
		} else {
			web_printf(p_link, ",");
		}
		if (p_curlink->server->ip_version == 6) {
			web_printf(p_link, "{\"ip_version\":\"%s\",\"peer_addr\":\"[%s]:%u\",\"local_addr\":\"[%s]:%u%s\",", \
					"IPv6", p_curlink->ip_peer, p_curlink->port_peer, p_curlink->ip_local, p_curlink->port_local, (p_curlink == p_link) ? "(this)" : "");
		} else {
			web_printf(p_link, "{\"ip_version\":\"%s\",\"peer_addr\":\"%s:%u\",\"local_addr\":\"%s:%u%s\",", \
					"IPv4", p_curlink->ip_peer, p_curlink->port_peer, p_curlink->ip_local, p_curlink->port_local, (p_curlink == p_link) ? "(this)" : "");
		}
		web_printf(p_link, "\"state\":\"");
		switch(p_curlink->state) {
			case STATE_RECVING:
				web_printf(p_link, "%s[%u]", "STATE_RECVING", p_curlink->recvbuf_len); break;
			case STATE_SUSPEND:
				web_printf(p_link, "%s", "STATE_SUSPEND"); break;
			case STATE_RESPONSING:
				web_printf(p_link, "%s", "STATE_RESPONSING"); break;
			case STATE_SENDING:
			case STATE_CLOSING:
				web_printf(p_link, "%s", "STATE_SENDING"); break;
			case STATE_CLOSED:
				web_printf(p_link, "%s", "STATE_CLOSED"); break;
			default:
				web_printf(p_link, "%s", "Unknown"); break;
		}
		web_printf(p_link, "\", \"ssl\":%s}", 
#ifdef WITH_SSL
			(p_curlink->server->is_ssl) ? "true" : "false"
#else
			"false"
#endif
			);
	}
	web_printf(p_link, "]}");
	web_fin(p_link, 200);
	return 0;
}
int http_streamingconfig(HTTP_FD *p_link) {
	web_printf(p_link, "{\"global_configs\": {\"aec\": 0, \"audio_healthy_threshold\": 1000, \"auto_start_push_play\": 1, \"bind_rules\": \"ALL=1;eth0:0=0;eth0:1=0;eth0:2=0;eth0:3=0;eth0:4=0;\", \"compatible_checked\": 1, \"demux_verbose_log\": 128006, \"dump_yuv\": 0, \"ec_param_set\": 2, \"enable_video_preview\": 0, \"hb_interval\": 1000, \"pbr_ratio\": 1000, \"pce_data_port\": 5051, \"pce_port\": 5050, \"psl_verbose_log\": 128006, \"ptcp_log\": 128006, \"pzvt_refresh_interval\": 60000, \"systemtime_changed_action\": 1, \"systemtime_changed_backward\": -15, \"systemtime_changed_forward\": 360000, \"vbv_ratio\": 1000, \"video_healthy_threshold\": 1000, \"ptcp_logsize\": 16024, \"psl_logsize\": 8016, \"demux_logsize\": 8016}, \"streaming_configs\": [{\"audio_capture_param\": {\"audio_channel_num\": 1, \"audio_sample_rate\": 48000, \"data_source\": 2000, \"impl\": 4}, \"video_capture_param\": {\"data_source\": 1000, \"device\": \"/dev/video0\", \"device_index\": 0, \"extern_param\": {\"name\": \"AndroidScreenCapturer\"}, \"format\": 2, \"fps\": 25, \"impl\": 12, \"library\": \"\", \"width\": 1920, \"height\": 1080}, \"streaming_param\": {\"delay_ms\": 3000, \"fps_report_interval\": 1000, \"audio_enc_param\": [{\"sample_rate\": 48000, \"channel_num\": 1, \"format\": 14, \"bitrate\": 64000, \"data_source\": 2000}], \"video_enc_param\": [{\"format\": 1, \"width\": 960, \"height\": 540, \"bitrate\": 1500, \"fps\": 25, \"key_interval_sec\": 2.0, \"enc_profile\": 2, \"adjust_br_min_ratio\": 25, \"adjust_br_max_ratio\": 110, \"adjust_fps_min_ratio\": 32, \"enc_bitrate_mode\": 2, \"enc_input_yuv_format\": 1, \"data_source\": 1000, \"codec_name\": \"\"}], \"enable_push\": 1, \"group\": \"testband\", \"profile\": \"PROFILE_0\", \"url\": \"http://172.16.0.100:7788/publish/trans/testband/<mlinkx>/<rid>?pdp=0&ndselect=704&arrc=2&quality=60&delay=300&linkdelay=500&livedelay=1000&linkmode=8&txmd=2&lpc=1&crs=1&ctrlmsg=1\"}, \"audio_enc_variant\": 1, \"group\": \"testband\", \"profile\": \"PROFILE_0\", \"working_mode\": 1}], \"enable_ebur\": 0, \"ec_param\": \"ec_agc1=1&ec_agc2=1&ec_agc=1&ec_da_suppression_bottom_line1=2000&ec_da_suppression_bottom_line2=2000&ec_da_suppression_bottom_line=2000&ec_fea_suppression1=1&ec_fea_suppression2=0&ec_fea_suppression=0&ec_fea_suppression_bottom_line1=400&ec_fea_suppression_bottom_line2=400&ec_fea_suppression_bottom_line=400&ec_fmp_hold_duration1=20&ec_fmp_hold_duration2=20&ec_fmp_hold_duration=20&ec_fmp_low_power_exit_threshold1=1000&ec_fmp_low_power_exit_threshold2=1000&ec_fmp_low_power_exit_threshold=1000&ec_fmp_low_power_threshold1=160&ec_fmp_low_power_threshold2=160&ec_fmp_low_power_threshold=160&ec_fmp_ne_low_power1=1&ec_fmp_ne_low_power2=0&ec_fmp_ne_low_power=0&ec_fmp_ne_non_activity1=0&ec_fmp_ne_non_activity2=0&ec_fmp_ne_non_activity=0&ec_fmp_ramp_duration1=10&ec_fmp_ramp_duration2=10&ec_fmp_ramp_duration=10&ec_fmp_ramp_exit_duration1=5&ec_fmp_ramp_exit_duration2=5&ec_fmp_ramp_exit_duration=5&ec_fmp_trigger_threshold1=50&ec_fmp_trigger_threshold2=50&ec_fmp_trigger_threshold=50&ec_force_farend_activity1=0&ec_force_farend_activity2=0&ec_force_farend_activity=0&ec_mobile1=0&ec_mobile2=0&ec_mobile=1&ec_nea_only_linear1=0&ec_nea_only_linear2=1&ec_nea_only_linear=0&ec_nea_update_linear_param1=1&ec_nea_update_linear_param2=1&ec_nea_update_linear_param=1&ec_ned_enr_exit_threshold1=400000&ec_ned_enr_exit_threshold2=1000000&ec_ned_enr_exit_threshold=100000&ec_ned_enr_threshold1=2000&ec_ned_enr_threshold2=9000&ec_ned_enr_threshold=2500&ec_ned_hold_duration1=50&ec_ned_hold_duration2=80&ec_ned_hold_duration=50&ec_ned_trigger_threshold1=20&ec_ned_trigger_threshold2=6&ec_ned_trigger_threshold=12&ec_nes_ban_comfort_noise1=1&ec_nes_ban_comfort_noise2=0&ec_nes_ban_comfort_noise=0&ec_ns1=1&ec_ns2=1&ec_ns=1&ec_turn_down_render1=7000&ec_turn_down_render2=7000&ec_turn_down_render=10000&ec_use_hw1=1&ec_use_hw2=0&ec_use_hw=1&ec_use_last_delay1=1&ec_use_last_delay2=1&ec_use_last_delay=0&mode1=1&mode2=1&mode=1&ns_force_sr1=0&ns_force_sr2=0&ns_force_sr=0&ns_level1=2&ns_level2=2&ns_level=2\"}");
	web_set_header(p_link, "Content-Type", "application/json");
	web_set_header(p_link, "Access-Control-Allow-Origin", "*");
	web_fin(p_link, 200);
	return 0;
}
#ifdef WITH_SSL
int http_ssl_ctx_cfg(HTTP_FD *p_link) {
	web_printf(p_link, \
		"<!DOCTYPE html>\r\n"\
		"<html>\r\n"\
		"<head>\r\n"\
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\r\n"\
		"<title>Server Configuration</title>\r\n"\
		"<style type=\"text/css\">\r\n"\
		"body{font-size:12px;font-family:\"Arial\";}\r\n"\
		".tb_title{margin-top:20px;text-align:center;font-weight:700;font-size:13px;}\r\n"\
		"table{margin:0px auto;border-bottom:1px solid gray;border-right:1px solid gray;}\r\n"\
		"table td{text-align:center;padding:2px 10px;border-top:1px solid gray;border-left:1px solid gray;}\r\n"\
		"table tr.tb_head td{font-weight:700;color:white;background-color:gray}\r\n"\
		"</style>\r\n"\
		"<script type=\"text/javascript\">\r\n"\
		"function form_submit(optype){\r\n"\
		"	document.getElementById(\"sslctx_form\").submit();\r\n"\
		"}\r\n"\
		"</script>\r\n"\
		"</head>\r\n"\
		"<body>\r\n"\
		"<form id=\"sslctx_form\" action=\"/cgi/sslCtxCfg\" method=\"post\" enctype=\"multipart/form-data\">\r\n"\
		"	<table>\r\n"\
		"		<tr>\r\n"\
		"			<td>Server Cert</td>\r\n"\
		"			<td><input type=\"file\" name=\"svr_certfile\" value=\"\" /></td>\r\n"\
		"		</tr>\r\n"\
		"		<tr>\r\n"\
		"			<td>Cert password</td>\r\n"\
		"			<td><input type=\"text\" name=\"password\" value=\"\" /></td>\r\n"\
		"		</tr>\r\n"\
		"		<tr>\r\n"\
		"			<td>Mutual Auth</td>\r\n");
	if (1) {
		web_printf(p_link, \
			"			<td><input type=\"checkbox\" name=\"mutual_auth\" value=\"1\" checked=\"checked\" /></td>\r\n");
	} else {
		web_printf(p_link, \
			"			<td><input type=\"checkbox\" name=\"mutual_auth\" value=\"1\" /></td>\r\n");
	}
	web_printf(p_link, \
		"		</tr>\r\n"\
		"		<tr>\r\n"\
		"			<td>CA Cert chain</td>\r\n"\
		"			<td><input type=\"file\" name=\"ca_certfile\" value=\"\" /></td>\r\n"\
		"		</tr>\r\n");
	web_printf(p_link, \
		"	</table>\r\n"\
		"	<input type=\"button\" value=\"Apply\" onclick=\"form_submit();\" />\r\n"\
		"</form>\r\n"\
		"</body>\r\n"\
		"</html>\r\n");
	web_fin(p_link, 200);
	return 0;
}
int http_cgi_ssl_ctx_cfg(HTTP_FD *p_link) {
	int ret = 0;
	const HTTP_FILES *p_certfile = NULL;
	const char *cert_pwd = NULL;
	// int mutual = 0;

	p_certfile = web_file_data(p_link, "svr_certfile");
	cert_pwd = web_post_str(p_link, "password", "");
	if (p_certfile != NULL && p_certfile->fname[0] != '\0') {
		if (cert_pwd[0] == '\0') {
			cert_pwd = NULL;
		}
		
	}
	// mutual = atoi(web_post_str(p_link, "mutual_auth", ""));
	p_certfile = web_file_data(p_link, "ca_certfile");
	if (p_certfile != NULL && p_certfile->fname[0] != '\0') {
		//ret = set_mutual_auth(mutual, (char *)(p_certfile->fcontent));
	} else {
		// ret = set_mutual_auth(mutual, NULL);
	}
	if (ret != 0) {
		printf("set_mutual_auth failed!\n");
		goto exit_fn;
	}
	//ssl_ctx_reload(); /* reload SSL context */
exit_fn:
	if (ret == 0) {
		printf("operate success!\n");
	}
	web_set_header(p_link, "Location", "/sslCtxCfg.html");
	web_fin(p_link, 302);
	return 0;
}
#endif
int httpcb_cgi_svrEnable(void *arg, int ret) {
	const char *retCode = "null";
	HTTP_FD *p_link = NULL;
	p_link = (HTTP_FD *)arg;
	retCode = (ret == 0) ? "success" : "failed";
	web_printf(p_link, "{\"retCode\":[\"%s\"]}", retCode);
	web_fin(p_link, 200);
	return 0;
}

#ifdef _WIN32
#else
int http_cgi_exec(HTTP_FD *p_link) {
	int cgi_output[2], cgi_input[2];
	pid_t pid;
	int status;
	char c;
	if (pipe(cgi_output) < 0) {
		web_fin(p_link, 500);
		return 0;
	}
	if (pipe(cgi_input) < 0) {
		web_fin(p_link, 500);
		return 0;
	}

	if ((pid = fork()) < 0) {
		web_fin(p_link, 500);
		return 0;
	}
	if (pid == 0) {
		dup2(cgi_output[1], 1);
		dup2(cgi_input[0], 0);
		close(cgi_output[0]);
		close(cgi_input[1]);
		execl(p_link->path + 1, p_link->path + 1, NULL);
		exit(0);
	} else {
		close(cgi_output[1]);
		close(cgi_input[0]);
		//write(cgi_input[1], &c, 1);
		while (read(cgi_output[0], &c, 1) > 0) {
			web_write(p_link, (unsigned char *)&c, 1);
		}
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
		printf("%s %d: status %d\n", __FILE__, __LINE__, status);
		web_fin(p_link, 200);
	}
	return 0;
}
int http_cgi_exec_bash(HTTP_FD *p_link) {
	int cgi_output[2], cgi_input[2];
	pid_t pid;
	int status;
	char c;
	if (pipe(cgi_output) < 0) {
		web_fin(p_link, 500);
		return 0;
	}
	if (pipe(cgi_input) < 0) {
		web_fin(p_link, 500);
		return 0;
	}

	if ((pid = fork()) < 0) {
		web_fin(p_link, 500);
		return 0;
	}
	if (pid == 0) {
		dup2(cgi_output[1], 1);
		dup2(cgi_input[0], 0);
		close(cgi_output[0]);
		close(cgi_input[1]);
		execl("/bin/bash", "bash", p_link->path + 1, NULL);
		exit(0);
	} else {
		close(cgi_output[1]);
		close(cgi_input[0]);
		//write(cgi_input[1], &c, 1);
		while (read(cgi_output[0], &c, 1) > 0) {
			web_write(p_link, (unsigned char *)&c, 1);
		}
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
		printf("%s %d: status %d\n", __FILE__, __LINE__, status);
		web_fin(p_link, 200);
	}
	return 0;
}
int http_cgi_exec_python(HTTP_FD *p_link) {
	int cgi_output[2], cgi_input[2];
	pid_t pid;
	int status;
	char c;
	if (pipe(cgi_output) < 0) {
		web_fin(p_link, 500);
		return 0;
	}
	if (pipe(cgi_input) < 0) {
		web_fin(p_link, 500);
		return 0;
	}

	if ((pid = fork()) < 0) {
		web_fin(p_link, 500);
		return 0;
	}
	if (pid == 0) {
		dup2(cgi_output[1], 1);
		dup2(cgi_input[0], 0);
		close(cgi_output[0]);
		close(cgi_input[1]);
		// execl("/usr/bin/python", "python", p_link->path + 1, NULL);
		execl("/usr/bin/python3", "python", p_link->path + 1, NULL);
		exit(0);
	} else {
		close(cgi_output[1]);
		close(cgi_input[0]);
		//write(cgi_input[1], &c, 1);
		while (read(cgi_output[0], &c, 1) > 0) {
			web_write(p_link, (unsigned char *)&c, 1);
		}
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
		printf("%s %d: status %d\n", __FILE__, __LINE__, status);
		web_fin(p_link, 200);
	}
	return 0;
}
#endif /* _WIN32 */

int http_call_system(HTTP_FD *p_link) {
	system("ping 127.0.0.1 -c 25");
	web_printf(p_link, "function system execute finished!\n");
	web_fin(p_link, 200);
	return 0;
}
int http_show_malloc(HTTP_FD *p_link) {
	web_printf(p_link, "<div>begin</div>\n");
#ifdef WATCH_RAM
	show_ram(1);
#endif
	web_printf(p_link, "<div>end</div>\n");
	web_fin(p_link, 200);
	return 0;
}
static int send_file(HTTP_FD *p_link) { /* not thread safe, because variable "content", only used by web server */
	SENDING_INFO *send_info = (SENDING_INFO *)p_link->userdata;
	HTTP_RANGE *p_range = send_info->range;
	FILE *fp = (FILE *)send_info->fp;
	size_t split_len = 0, read_len = 0;
	static unsigned char *content = NULL;

	if (content == NULL) {
		content = (unsigned char *)malloc(FILE_BUFFER_SIZE);
	}
	if (p_range != NULL) {
		if (send_info->offset == 0) { /* content sending not start yet */
			split_len = p_range->end - p_range->start + 1;
		} else {
			split_len = p_range->end - send_info->offset + 1;
		}
	} else {
		split_len = send_info->size;
	}
	read_len = FILE_BUFFER_SIZE;
	if (read_len > split_len) {
		read_len = split_len;
	}
	p_link->response_entity.used = 0;
	if (send_info->offset == 0) { /* content sending not start yet */
		if (send_info->boundary != NULL) { /* multi range defined */
			web_printf(p_link, "--%s\r\n", send_info->boundary);
			web_printf(p_link, "Content-Type: %s\r\n", send_info->mime_type);
			web_printf(p_link, "Content-Range: bytes %" PRId64 "-%" PRId64 "/%" PRId64 "\r\n\r\n", p_range->start, p_range->end, send_info->size);
		}
		if (p_range != NULL) {
			send_info->offset = p_range->start;
		}
	}
#ifdef _WIN32
	_fseeki64(fp, send_info->offset, SEEK_SET);
#else
	fseek(fp, send_info->offset, SEEK_SET);
#endif
	// printf("fread, offset:%zu size:%zu\n", send_info->offset, read_len);
	fread(content, read_len, 1, fp);
	send_info->offset += read_len;
	web_write(p_link, content, read_len);
	p_link->state = STATE_SENDING;
	p_link->sending_len = read_len;
	p_link->send_state = SENDING_ENTITY;
	if ((p_range == NULL && send_info->offset == send_info->size) || (p_range != NULL && send_info->offset == p_range->end + 1)) {
		if (send_info->boundary != NULL) { /* multi range defined */
			web_printf(p_link, "\r\n");
		}
		send_info->offset = 0;
		if (p_range != NULL) {
			send_info->range = p_range->next;
		}
		if (send_info->range == NULL) { /* last range send complete, response complete */
			if (send_info->boundary != NULL) { /* multi range defined */
				web_printf(p_link, "--%s--\r\n", send_info->boundary);
			}
			p_link->send_cb = NULL;
		}
	}
	return 0;
}
static void send_info_close(void *_send_info) {
	SENDING_INFO *send_info = (SENDING_INFO *)_send_info;
	if (send_info->fp != NULL) {
		fclose(send_info->fp);
	}
	if (send_info->mime_type != NULL) {
		free(send_info->mime_type);
	}
	if (send_info->boundary != NULL) {
		free(send_info->boundary);
	}
	free(send_info);
}
int http_send_file(HTTP_FD *p_link) {
	int resp_code = 200;
	size_t con_len = 0;
	char *fullpath = NULL, str_temp[128], str_conlen[21];
	SENDING_INFO *send_info = NULL;
	HTTP_RANGE *p_range = NULL;
	const char *mime_type = NULL;

	if (p_link->server->root == NULL) {
		web_fin(p_link, 500);
		return 0;
	}
	fullpath = (char *)malloc(strlen(p_link->server->root) + strlen(p_link->path) + 2);
	if (fullpath == NULL) {
		web_fin(p_link, 500);
		return 0;
	}
	strcpy(fullpath, p_link->server->root);
	if (*(fullpath + strlen(fullpath)) != '/') {
		strcat(fullpath, "/");
	}
	if (p_link->path[0] == '/') {
		strcat(fullpath, p_link->path + 1);
	} else {
		strcat(fullpath, p_link->path);
	}

	send_info = (SENDING_INFO *)malloc(sizeof(SENDING_INFO));
	if (send_info == NULL) {
		resp_code = 500;
		goto exit;
	}
	memset(send_info, 0x00, sizeof(SENDING_INFO));
	send_info->fp = fopen(fullpath, "rb");
	if (send_info->fp == NULL) {
		resp_code = 404;
		goto exit;
	}
#ifdef _WIN32
	_fseeki64((FILE *)send_info->fp, 0, SEEK_END);
	send_info->size = _ftelli64((FILE *)send_info->fp);
#else
	fseek((FILE *)send_info->fp, 0, SEEK_END);
	send_info->size = ftell((FILE *)send_info->fp);
#endif
	for (p_range = p_link->range_data; p_range != NULL; p_range = p_range->next) {
		if (p_range->end == RANGE_NOTSET) {
			p_range->end = send_info->size - 1;
		} else if (p_range->start == RANGE_NOTSET) {
			p_range->start = send_info->size - p_range->end;
			p_range->end = send_info->size - 1;
		}
		if (p_range->end < p_range->start || p_range->end >= send_info->size) {
printf("will response 416, %s %d, %" PRId64 " to %" PRId64 " / %" PRId64 "\n", __FILE__, __LINE__, p_range->start, p_range->end, send_info->size);
			resp_code = 416;
			goto exit;
		}
	}

	if (p_link->range_data == NULL) {
		resp_code = 200;
		sprintf(str_conlen, "%" PRId64, send_info->size);
	} else if (p_link->range_data->next == NULL) {
		resp_code = 206;
		p_range = p_link->range_data;
		snprintf(str_temp, sizeof(str_temp) - 1, "bytes %" PRId64 "-%" PRId64 "/%" PRId64, p_range->start, p_range->end, send_info->size);
		web_set_header(p_link, "Content-Range", str_temp);
		sprintf(str_conlen, "%" PRId64, p_range->end - p_range->start + 1);
	} else {
		resp_code = 206;
		mime_type = get_mime_type(p_link->path);
		send_info->mime_type = strdup(mime_type);

		snprintf(str_temp, sizeof(str_temp) - 1, "ip:%s,port:%u,time:%ld,rand:%d", p_link->ip_peer, p_link->port_peer, clock(), rand());
		send_info->boundary = (char *)malloc(41);
		if (send_info->boundary == NULL) {
			resp_code = 500;
			goto exit;
		}
		tt_sha1_hex((unsigned char *)str_temp, strlen(str_temp), send_info->boundary);
		snprintf(str_temp, sizeof(str_temp) - 1, "multipart/byteranges; boundary=%s", send_info->boundary);
		web_set_header(p_link, "Content-Type", str_temp);
		con_len = 0;
		for (p_range = p_link->range_data; p_range != NULL; p_range = p_range->next) {
			con_len += 2 + strlen(send_info->boundary) + 2; /* "--boundary\r\n" */
			con_len += strlen("Content-Type: ") + strlen(mime_type) + 2; /* "Content-Type: mime_type\r\n" */
			snprintf(str_temp, sizeof(str_temp) - 1, "bytes %" PRId64 "-%" PRId64 "/%" PRId64, p_range->start, p_range->end, send_info->size);
			con_len += strlen("Content-Range: ") + strlen(str_temp) + 2; /* "Content-Range: bytes start-end/size\r\n" */
			con_len += 2 + (p_range->end - p_range->start + 1) + 2; /* \r\ncontent\r\n" */
		}
		con_len += 2 + strlen(send_info->boundary) + 4; /* "--boundary--\r\n" */
		sprintf(str_conlen, "%" PRId64, con_len);
	}

	send_info->range = p_link->range_data;
	p_link->userdata = send_info;
	p_link->free_userdata = send_info_close;

	web_set_header(p_link, "Content-Length", str_conlen);
	p_link->send_cb = send_file;
exit:
	web_fin(p_link, resp_code);
	if (fullpath != NULL) {
		free(fullpath);
	}
	return 0;
}
#ifdef WITH_WEBSOCKET
int http_ws_test(HTTP_FD *p_link, E_WS_EVENT event) {
	switch (event) {
		case EVENT_ONOPEN:
			printf("websocket [%s] %s:%u ONOPEN.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONMESSAGE:
			printf("websocket [%s] %s:%u ONMESSAGE.\n", p_link->path, p_link->ip_peer, p_link->port_peer);
			ws_printf(p_link, "received %" PRId64 " bytes: \"%s\"", p_link->ws_data.used, p_link->ws_data.content);
			ws_pack(p_link, WS_OPCODE_TEXT);
			break;
		case EVENT_ONCLOSE:
			printf("websocket [%s] %s:%u ONCLOSE.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONERROR:
			printf("websocket [%s] %s:%u ONERROR.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONPING:
			printf("websocket [%s] %s:%u ONPING.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONPONG:
			printf("websocket [%s] %s:%u ONPONG.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONCHUNKED:
			printf("websocket [%s] %s:%u ONCHUNKED.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		default:
			printf("websocket [%s] %s:%u unknown event.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
	}
	return 0;
}
static int save_dump(HTTP_FD *p_link) {
	FILE *out_file = NULL;
	const char *fname = NULL;

	fname = web_query_str(p_link, "name", "h264dump.data");
	out_file = fopen(fname, "ab");
	if (out_file == NULL) {
		return -1;
	}
	fwrite(p_link->ws_data.content, p_link->ws_data.used, 1, out_file);
	fclose(out_file);
	return 0;
}
int http_ws_dump(HTTP_FD *p_link, E_WS_EVENT event) {
	switch (event) {
		case EVENT_ONOPEN:
			printf("websocket [%s] %s:%u ONOPEN.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONMESSAGE:
			printf("websocket [%s] %s:%u ONMESSAGE.\n", p_link->path, p_link->ip_peer, p_link->port_peer);
			save_dump(p_link);
			break;
		case EVENT_ONCLOSE:
			printf("websocket [%s] %s:%u ONCLOSE.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONERROR:
			printf("websocket [%s] %s:%u ONERROR.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONPING:
			printf("websocket [%s] %s:%u ONPING.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONPONG:
			printf("websocket [%s] %s:%u ONPONG.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONCHUNKED:
			printf("websocket [%s] %s:%u ONCHUNKED.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		default:
			printf("websocket [%s] %s:%u unknown event.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
	}
	return 0;
}
double get_time() {
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
}
typedef struct PerfInfo {
	int32_t flags;
	FILE *fin;
	size_t fsize;
	size_t foffset;
	double tm_start; /* sec */
	double speed; /* kbps */
	int64_t sended_5s; /* Bytes */
	uint8_t *payload;
	size_t payload_size;
} PerfInfo;
static void perfinfo_close(void *_perf_info) {
	PerfInfo *perf_info = (PerfInfo *)_perf_info;
	if (perf_info == NULL) {
		return;
	}
	if (perf_info->fin != NULL) {
		fclose(perf_info->fin);
		printf("[%.3lf] .h264 closed\n", get_time());
	}
	if (perf_info->payload != NULL) {
		free(perf_info->payload);
	}
	free(perf_info);
}
int ws_perf_send_data() {
	HTTP_FD *p_curlink = NULL;
	double now = 0;
	PerfInfo *perf_info = NULL;
	size_t read_size = 0;

	for (p_curlink = g_http_links; p_curlink != NULL; p_curlink = p_curlink->next) {
		if (p_curlink->state != STATE_WS_CONNECTED) {
			continue;
		}
		perf_info = session_get_storage(p_curlink->session, "perf", NULL);
		if (perf_info == NULL) {
			continue;
		}
		now = get_time();
		if (now < perf_info->tm_start + 0.001) {
			continue;
		}
		while (perf_info->sended_5s / (now - perf_info->tm_start) < perf_info->speed) {
			fseek(perf_info->fin, perf_info->foffset, SEEK_SET);
			read_size = perf_info->fsize - perf_info->foffset;
			if (read_size > perf_info->payload_size) {
				read_size = perf_info->payload_size;
			}
			if (perf_info->flags & 1) {
				fread(perf_info->payload, read_size, 1, perf_info->fin);
			} else {
				memset(perf_info->payload, 0x00, read_size);
			}
			perf_info->foffset += read_size;
			if (perf_info->foffset == perf_info->fsize) {
				printf("[%.3lf] EOF %s\n", get_time(), web_query_str(p_curlink, "fname", "test_20Mbps.h264"));
				perf_info->foffset = 0;
			}
			ws_write(p_curlink, perf_info->payload, read_size);
			ws_pack(p_curlink, WS_OPCODE_BINARY);
			perf_info->sended_5s += read_size;
		}
		if (now > perf_info->tm_start + 5.0) { /* restart statistics */
			perf_info->tm_start = now;
			perf_info->sended_5s = 0;
		}
	}
	return 0;
}
int http_ws_perf(HTTP_FD *p_link, E_WS_EVENT event) {
	PerfInfo *perf_info = NULL;
	switch (event) {
		case EVENT_ONOPEN:
			perf_info = (PerfInfo *)malloc(sizeof(PerfInfo));
			if (perf_info != NULL) {
				memset(perf_info, 0x00, sizeof(PerfInfo));
				perf_info->flags = atoi(web_query_str(p_link, "flags", "65535"));
				perf_info->speed = atof(web_query_str(p_link, "kbps", "20000")) * (1024 / 8);
printf("speed %.3lf\n", perf_info->speed);
				perf_info->payload_size = 1024;
				perf_info->payload = (uint8_t *)malloc(perf_info->payload_size);
				perf_info->fin = fopen(web_query_str(p_link, "fname", "test_20Mbps.h264"), "rb");
				perf_info->tm_start = get_time();
				if (perf_info->fin != NULL) {
					fseek(perf_info->fin, 0, SEEK_END);
					perf_info->fsize = ftell(perf_info->fin);
					fseek(perf_info->fin, 0, SEEK_SET);
					if (perf_info->fsize > 0) {
						session_set_storage(p_link->session, "perf", perf_info, perfinfo_close);
						printf("[%.3lf] .h264 opened\n", get_time());
					} else {
						perfinfo_close(perf_info);
					}
				} else {
					perfinfo_close(perf_info);
				}
			}
			printf("websocket [%s] %s:%u ONOPEN.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONMESSAGE:
			printf("websocket [%s] %s:%u ONMESSAGE.\n", p_link->path, p_link->ip_peer, p_link->port_peer);
			save_dump(p_link);
			break;
		case EVENT_ONCLOSE:
			session_unset_storage(p_link->session, "perf");
			printf("websocket [%s] %s:%u ONCLOSE.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONERROR:
			printf("websocket [%s] %s:%u ONERROR.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONPING:
			printf("websocket [%s] %s:%u ONPING.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONPONG:
			printf("websocket [%s] %s:%u ONPONG.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		case EVENT_ONCHUNKED:
			printf("websocket [%s] %s:%u ONCHUNKED.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
		default:
			printf("websocket [%s] %s:%u unknown event.\n", p_link->path, p_link->ip_peer, p_link->port_peer); break;
	}
	return 0;
}
#endif

int http_callback_default(HTTP_FD *p_link) {
	TT_FILE *ram_file = NULL;
	char file_etag[36];
	const char *p_char = NULL, *p_suffix = "";
	int pre_is_slash = 0;

	ram_file = get_file(p_link->path);
	if (ram_file == NULL) {
		http_send_file(p_link);
		return 0;
	}
	// get suffix of resource
	for (p_char = p_link->path; *p_char != '\0'; p_char++) {
		if (pre_is_slash) {
			pre_is_slash = 0;
			continue;
		}
		if (*p_char == '/') {
			pre_is_slash = 1;
			p_suffix = "";
		} else if (*p_char == '.' && *(p_char + 1) != '\0') {
			p_suffix = p_char;
		}
	}
	sprintf(file_etag, "\"%s\"", ram_file->sz_md5);

	// set cache policy
	if (0 == strcmp(p_suffix, ".html")) {
#if 0
		web_set_header(p_link, "Cache-Control", "public, max-age=30"); //html cached 30 seconds
		web_set_header(p_link, "Pragma", NULL);
	} else if (0 == strcmp(p_suffix, ".js")) {
		web_set_header(p_link, "Cache-Control", "public, max-age=120"); //js cached 2 minutes
		web_set_header(p_link, "Pragma", NULL);
	} else if (0 == strcmp(p_suffix, ".gif") || 0 == strcmp(p_suffix, ".jpg") || 0 == strcmp(p_suffix, ".jpeg") || 0 == strcmp(p_suffix, ".png")) {
		web_set_header(p_link, "Cache-Control", "public, max-age=120"); // picture cached 2 minutes
		web_set_header(p_link, "Pragma", NULL);
	} else if (0 == strcmp(p_suffix, ".css")) {
		web_set_header(p_link, "Cache-Control", "public, max-age=300"); //css cached 5 minutes
		web_set_header(p_link, "Pragma", NULL);
	} else if (0 == strcmp(p_suffix, ".woff") || 0 == strcmp(p_suffix, ".ttf")) {
		web_set_header(p_link, "Cache-Control", "public, max-age=300"); // font cached 5 minutes
		web_set_header(p_link, "Pragma", NULL);
	} else {// not cached for other resource
#endif
	}
	if (0 != strcmp(file_etag, web_header_str(p_link, "If-None-Match", ""))) {
		web_set_header(p_link, "Etag", file_etag);
		if (ram_file->is_gzip) {
			web_set_header(p_link, "Content-Encoding", "gzip");
			web_no_copy(p_link, ram_file->p_content, ram_file->u_size, 0, 0);
			web_fin(p_link, 200);
		} else {
			web_no_copy(p_link, ram_file->p_content, ram_file->u_size, 0, 0);
			web_fin(p_link, 200);
		}
	} else {
		web_fin(p_link, 304);
	}
	return 0;
}

int msg_test(const char *name, void *buf, size_t len) {
	printf("%s %p %" PRId64 "\n", name, buf, len);
	return 0;
}

extern int http_download_cert(HTTP_FD *p_link);
void tt_handler_register() {
	tt_handler_init();
	tt_handler_add("/", http_root);
	tt_handler_add("/cgi/menu.json", http_cgi_menu);
	tt_handler_add("/cgi/signin.json", http_cgi_signin);
	tt_handler_add("/cgi/signout.json", http_cgi_signout);
	tt_handler_add("/cgi/testGet.json", http_test_get);
	tt_handler_add("/cgi/testPost.json", http_test_post);
	tt_handler_add("/cgi/testUpload.json", http_test_upload);
	tt_handler_add("/cgi/testUploadLarge.json", http_test_upload_large);
	tt_handler_add("/cgi/testDownload.json", http_test_download);
	tt_handler_add("/cgi/svrConf.json", http_svr_conf);
	tt_handler_add("/cgi/svrList.json", http_svr_list);
	tt_handler_add("/cgi/sessionList.json", http_session_list);
	tt_handler_add("/cgi/linkList.json", http_link_list);
	tt_handler_add("/rooms/memberships/streaming_config/", http_streamingconfig);
#ifdef WITH_SSL
	tt_handler_add("/sslCtxCfg.html", http_ssl_ctx_cfg);
	tt_handler_add("/cgi/sslCtxCfg", http_cgi_ssl_ctx_cfg);
#endif
#ifndef _WIN32
	tt_handler_add("/cgi_exec", http_cgi_exec);
	tt_handler_add("/cgi_exec.sh", http_cgi_exec_bash);
	tt_handler_add("/cgi_exec.py", http_cgi_exec_python);
#endif
	tt_handler_add("/call_system", http_call_system);
	tt_handler_add("/showMalloc.html", http_show_malloc);
	// tt_handler_print();

#ifdef WITH_WEBSOCKET
	tt_ws_handler_init();
	tt_ws_handler_add("/ws/test", http_ws_test);
	tt_ws_handler_add("/ws/dump", http_ws_dump);
	tt_ws_handler_add("/ws/perf", http_ws_perf);
#endif

	tt_msg_handler_init();
	tt_msg_handler_add("test", msg_test);
}
/* dispatch all http request */
int req_dispatch(HTTP_FD *p_link) {
	const char *no_auth[] = {"/", "/signin.html", "/cgi/signin.json", \
		"/js/vue.js", "/js/vue.min.js", "/js/vue-router.js", "/js/vue-router.min.js", "/js/axios.min.js", "/js/elementui.js", "/js/myapp.js", \
		"/favicon.ico", "/css/elementui.css", "/css/myapp.css", "/css/fonts/element-icons.woff", "/css/fonts/element-icons.ttf", \
		"/cgi_exec", "/cgi_exec.sh", "/cgi_exec.py", "/call_system", \
		NULL};
	int i = 0, need_auth = 1;
	int (*callback)(HTTP_FD *p_link);

	if (0 != set_session(p_link)) {
		return 0;
	}
	web_set_header(p_link, "Alt-Svc", "h2=\":20443\"; ma=2592000");
#if 1 /* auto login as anonymous */
	if (!p_link->session->isonline) {
		printf("user \"anonymous\" login.\n");
		session_login(p_link->session, "anonymous", 99);
	}
	for (i = 0; no_auth[i] != NULL;i++) {
		if (0 == strcmp(p_link->path, no_auth[i])) {
			need_auth = 0;
			break;
		}
	}
#endif

	if (!p_link->session->isonline) {
		if (need_auth) {
			web_printf(p_link, "<script type=\"text/javascript\">window.top.location.href = \"/signin.html\";</script>");
			web_set_header(p_link, "Content-Type", "text/html");
			web_fin(p_link, 200);
			return 0;
		}
	} else {
		if (0 == strcmp(p_link->path, "/signin.html")) {
			web_printf(p_link, "<script type=\"text/javascript\">window.top.location.href = \"/\";</script>");
			web_set_header(p_link, "Content-Type", "text/html");
			web_fin(p_link, 200);
			return 0;
		}
	}

#ifdef WITH_WEBSOCKET
	if (tt_ws_handler_get(p_link->path) != NULL) {
		if (0 == ws_handshake(p_link)) { /* websocket handshake success */
			return 0;
		}
	}
#endif
	callback = tt_handler_get(p_link->path);
	if (callback) {
		callback(p_link);
	} else {
		http_callback_default(p_link);
	}
	if (p_link->state != STATE_SUSPEND && p_link->sending_len == 0) {
		printf("return without call \"web_fin\" @ \"%s\"!\n", p_link->path);
		web_fin(p_link, 200);
	}
	update_session_expires(p_link);
	return 0;
}

#ifdef WITH_WEBSOCKET
int ws_dispatch(HTTP_FD *p_link, E_WS_EVENT event) {
	int (*callback)(HTTP_FD *p_link, E_WS_EVENT) = NULL;

	callback = tt_ws_handler_get(p_link->path);
	if (callback) {
		callback(p_link, event);
	}
	update_session_expires(p_link);
	return 0;
}
#endif

int msg_dispatch(const char *name, void *buf, size_t len) {
	int (*callback)(const char *name, void *buf, size_t len);

	callback = tt_msg_handler_get(name);
	if (callback) {
		return callback(name, buf, len);
	}
	return -1;
}
