#coding:utf-8
import os
import hashlib
import struct
import sys
import gzip, io

g_pack_path = "./static"

g_tree = [] #文件树结构，保存以便于排序或其他操作
g_tree_serial = [{"type":"D","name":"/","path":"","size":0,"md5":"","parent":0,"brother":0,"children":0,"children_len":0}] #序列化后的树信息
g_head_pool = b"" #文件头信息（当前用网络序的unsigned int保存共有多少个文件/文件夹，方便提取文件树）
g_tree_pool = b"" #保存文件树（顺便计算MD5值）
g_fname_pool = b"" #保存文件名
g_fcontent_pool = b"" #保存文件内容（顺便进行gzip压缩）
g_head_len = 16
g_tree_len = 0
g_fname_len = 0
def get_tree(posi):
	tree = []
	li = os.listdir(posi)
	for fname in li:
		cur_sub = posi + "/" + fname
		if fname.startswith("."): #跳过以'.'开头的文件
			continue
		if os.path.splitext(fname)[1] in [".py", ".pl", ".txt", ".htc"]: #跳过.py, .pl, .txt, .htc的文件
			continue
		fsize = 0;
		if os.path.isfile(cur_sub):
			tree.append({"type":"F","posi":posi,"name":fname,"children":[]})
		elif os.path.isdir(cur_sub):
			tree.append({"type":"D","posi":posi,"name":fname,"children":get_tree(cur_sub)})
		else:
			print("Unknow type: '" + cur_sub + "'.")
			continue
	tree.sort(key=lambda x: x["name"]) # 按照名称进行排序，便于二分查找
	return tree

def tree_serial(parent):
	for finfo in g_tree:
		finfo["parent"] = 0
	cur_level = g_tree

	while len(cur_level) != 0:
		next_level = []
		for finfo in cur_level:
			if finfo["type"] == "F":
				cur_sub = finfo["posi"] + "/" + finfo["name"]
				fsize = os.path.getsize(cur_sub)
			else:
				fsize = 0

			# 如果父亲的第一孩子节点没设置，则设置为当前节点，如果已经设置了，就设置上一个节点的弟弟为当前节点
			if g_tree_serial[finfo["parent"]]["children"] == 0:
				g_tree_serial[finfo["parent"]]["children"] = len(g_tree_serial)
			else:
				g_tree_serial[len(g_tree_serial) - 1]["brother"] = len(g_tree_serial)
			g_tree_serial[finfo["parent"]]["children_len"] += 1
			# 当前节点加入到树信息中
			g_tree_serial.append({"type":finfo["type"],"posi":finfo["posi"],"name":finfo["name"],"size":fsize,"md5":"","gzip":'\0',"parent":finfo["parent"],"brother":0,"children":0,"children_len":0})
			if len(finfo["children"]) != 0:
				for sub_dir in finfo["children"]:
					sub_dir["parent"] = len(g_tree_serial) - 1
				next_level += finfo["children"]
		cur_level = next_level

g_tree = get_tree(g_pack_path)
#print(g_tree)
tree_serial(0) # 将g_tree序列化为g_tree_serial
g_head_pool = struct.pack("!4I", len(g_tree_serial), 0, 0, 0)
g_tree_len = len(g_tree_serial) * 40 #每一个文件信息占40个字节
for finfo in g_tree_serial:
	g_fname_len += len(finfo["name"].encode()) + 1 #每一个文件名都以'\0'结尾
f_log = open("package.log", "wb")
for finfo in g_tree_serial:
	log_entry = "%s %s\t%d\t"%(finfo["type"], finfo["name"], finfo["size"])
	log_entry += "P%s\t"%(g_tree_serial[finfo["parent"]]["name"])
	log_entry += "N%s\t"%(g_tree_serial[finfo["brother"]]["name"])
	log_entry += "C%s"%(g_tree_serial[finfo["children"]]["name"])
	log_entry += "\r\n"
	sys.stdout.write(log_entry)
	f_log.write(log_entry.encode())

	if finfo["type"] == 'F':
		fin = open(finfo["posi"] + "/" + finfo["name"], "rb")
		fcontent = fin.read()
		fin.close()
		if len(fcontent) != finfo["size"]:
			print("fsize changed!")
			exit(0)
		#finfo["md5"] = hashlib.md5(fcontent).hexdigest()
		finfo["md5"] = hashlib.md5(fcontent).digest() #md5值使用文件原始内容计算
		if os.path.splitext(finfo["name"])[1] in [".html", ".htm", ".xhtml", ".js", ".css"]:
			# 对html, js, css文件进行gzip压缩，但index.html不压缩
			if finfo["name"] != "index.html":
				output_io = io.BytesIO()
				gzip_fd = gzip.GzipFile(fileobj=output_io, mode='wb', compresslevel=9)
				gzip_fd.write(fcontent)
				gzip_fd.close()
				fcontent = output_io.getvalue()
				finfo["size"] = len(fcontent)
				finfo["gzip"] = '\x01'
		fcontent_offset = g_head_len + g_tree_len + g_fname_len + len(g_fcontent_pool)
	# 感叹号表示数字都以网络序存储，c表示一个字符，L表示unsigned long的数据，5L表示5个unsigned long的数据
	# 文件名字符串的偏移量先用65535表示
	# 对应结构体：
	# struct ST_TREE_INFO{
	# 	unsigned int fname_offset; //文件名/文件夹名相对文件头的偏移量
	# 	unsigned int fcontent_offset; //文件内容相对文件头的偏移量，如果是文件夹为0
	# 	unsigned int fsize; //文件内容大小，如果是文件夹为0
	# 	unsigned int parent_id; //父文件夹的索引
	# 	unsigned int brother_id; //兄弟的索引
	# 	unsigned int child_id; //第一个孩子的索引
	# 	unsigned int children_len; //孩子的数量
	# 	char ftype; //'F'表示文件，'D'表示文件夹
	# 	char md5[33]; //文件的MD5值，如果是文件夹为空字符串
	# }
	if finfo["type"] == 'F':
		g_tree_pool += struct.pack("!4c5I16s", finfo["type"].encode(), finfo["gzip"].encode(), b'\0', b'\0', g_head_len + g_tree_len + len(g_fname_pool), fcontent_offset, finfo["size"], finfo["parent"], finfo["brother"], finfo["md5"])
	else:
		g_tree_pool += struct.pack("!4c5I16s", finfo["type"].encode(), b'\0', b'\0', b'\0', g_head_len + g_tree_len + len(g_fname_pool), finfo["children"], finfo["children_len"], finfo["parent"], finfo["brother"], b"")
	g_fname_pool += finfo["name"].encode() + b"\0"
	if finfo["type"] == 'F':
		g_fcontent_pool += fcontent
f_log.close()

fout = open("package.bin", "wb")
fout.write(g_head_pool)
fout.write(g_tree_pool)
fout.write(g_fname_pool)
fout.write(g_fcontent_pool)
print(struct.pack("!I", len(g_head_pool) + len(g_tree_pool) + len(g_fname_pool) + len(g_fcontent_pool) + 4))
fout.write(struct.pack("!I", len(g_head_pool) + len(g_tree_pool) + len(g_fname_pool) + len(g_fcontent_pool) + 4))
fout.close()
