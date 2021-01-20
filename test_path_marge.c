#include <stdio.h>
#include <string.h>

/*
../aaa => /aaa
/../aaa => /aaa
/aaa/../bbb => /bbb
aaa/./bbb => /aaa/bbb
aaa/../bbb => /bbb
aaa/bbb/ccc => aaa/bbb/ccc
*/
static int path_merge(char *dst, const char *src) {
	const char *p_start = NULL, *p_end = NULL;
	char *p_dstchar = NULL;

	for (p_dstchar = dst, p_end = p_start = src; ; ) {
		if (*p_end == '/' || *p_end == '\0') { /* TODO for windows '\\' */
			if (p_end == p_start + 2 && *p_start == '.' && *(p_start + 1) == '.') { /* ../ */
				if (p_dstchar > dst && *(p_dstchar - 1) == '/') {
					p_dstchar -= 1;
				}
				while (1) {
					if (p_dstchar == dst) {
						*p_dstchar = '/';
						p_dstchar++;
						break;
					} else if (*(p_dstchar - 1) == '/') {
						break;
					} else {
						p_dstchar--;
					}
				}
			} else if (p_end == p_start + 1 && *p_start == '.') { /* ./ */
				if (*dst != '/') {
					memmove(dst + 1, dst, p_dstchar - dst + 1);
					p_dstchar++;
					*dst = '/';
				}
			} else {
				memcpy(p_dstchar, p_start, p_end - p_start + 1);
				p_dstchar += p_end - p_start + 1;
			}
			if (*p_end == '\0') {
				break;
			}
			p_end++;
			p_start = p_end;
			continue;
		}
		p_end++;
	}
	*p_dstchar = '\0';
	return 0;
}

int main() {
	char test1[40] = "../aaa";
	char test2[40] = "/../aaa";
	char test3[40] = "/aaa/../bbb";
	char test4[40] = "aaa/./bbb";
	char test5[40] = "aaa/../bbb";
	char test6[40] = "http://19.44.57:75/aaa/bbb/ccc";
	path_merge(test1, test1);
	printf("%s\n", test1);
	path_merge(test2, test2);
	printf("%s\n", test2);
	path_merge(test3, test3);
	printf("%s\n", test3);
	path_merge(test4, test4);
	printf("%s\n", test4);
	path_merge(test5, test5);
	printf("%s\n", test5);
	path_merge(test6, test6);
	printf("%s\n", test6);
	return 0;
}
