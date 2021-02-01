axios.defaults.headers.post["Content-Type"] = "application/x-www-form-urlencoded";
axios.defaults.headers.get["Content-Type"] = "application/x-www-form-urlencoded";
axios.defaults.transformRequest = [function(data, headers){
	if (typeof(data) == "undefined") {
		return "";
	}
	if (headers["Content-Type"] == "multipart/form-data") {
		return data;
	}
	if (typeof(headers["Content-Type"] == "undefined") || headers["Content-Type"] == "application/x-www-form-urlencoded") {
		var params = new URLSearchParams();
		for (var it in data) {
			if (typeof(data[it]) == "boolean") {
				if (data[it]) {
					params.append(it, 1);
				}
			} else {
				params.append(it, data[it]);
			}
		}
		return params;
	}
}]

axios.upload = function(url, data, config) {
	var instance = axios.request({
		url: url,
		method: "post",
		data: data,
		processData: false,
		headers:{'Content-Type':'multipart/form-data'}
	});
	return instance;
}

Vue.component("container", {
	template:
		"<el-container style=\"height:100%;\">" +
		"	<el-header style=\"background-color:#3a8adc;\">" +
		"		<el-row style=\"height:100%;\">" +
		"			<el-col :span=\"8\" style=\"height:100%;\">&nbsp;</el-col>" +
		"			<el-col :span=\"8\" style=\"height:100%;\">&nbsp;</el-col>" +
		"			<el-col :span=\"8\" style=\"height:100%;\">" +
		"				<div style=\"line-height:60px;text-align:right;\"><el-button type=\"text\" style=\"color:white;\" @click=\"signout()\">Sing Out</el-button></div>" +
		"			</el-col>" +
		"		</el-row>" +
		"	</el-header>" +
		"	<el-container>" +
		"		<el-aside width=\"200px\"><slot name=\"aside\"></slot></el-aside>" +
		"		<el-main><slot ></slot></el-main>" +
		"	</el-container>" +
		"</el-container>",
	methods: {
		signout: function() {
			axios.post("/cgi/signout.json", {"signout":1}).then(resp => {
				if (resp.data["retCode"][0] == "success") {
					window.top.location.href = "/";
				} else {
					this.$message.error(resp.data["retCode"][0]);
				}
			}).catch(error => this.$message.error(error.message));
		}
	}
	});

Vue.component("navmenu", {
		props: ["prefix", "navmenus"],
		template:
			"<div>" +
			"	<template v-for=\"(navmenu, index) in navmenus\">" +
			"	<el-submenu v-if=\"navmenu.hasOwnProperty('children')\" :index=\"prefix + String(index)\" :key=\"index\">" +
			"		<template v-slot:title>{{navmenu.label}}</template>" +
			"		<navmenu :prefix=\"prefix + String(index) + '-'\" :navmenus=\"navmenu.children\"></navmenu>" +
			"	</el-submenu>" +
			"	<el-menu-item v-else :index=\"navmenu.url\" :key=\"index\">{{navmenu.label}}</el-menu-item>" +
			"	</template>" +
			"</div>",
	});

Vue.component("allmenu", {
		template:
			"<el-menu :default-active=\"menuActive\" :default-openeds=\"menuOpeneds\" active-text-color=\"#409eff\" style=\"padding-top:30px;\" @select=\"menuSelect\" @open=\"menuChange\" @close=\"menuChange\">" +
			"	<navmenu prefix=\"item-\" :navmenus=\"menuData\"></navmenu>" +
			"</el-menu>",
		data: function (){
			return {
				menuActive: "",
				menuData: "",
				menuOpeneds: []
			}
		},
		created: function() {
			axios.get("/cgi/menu.json").then(resp => {
					this.menuActive = document.location.pathname;
					this.menuData = resp.data["menuData"];
					this.menuOpeneds = JSON.parse(window.localStorage.getItem("menuOpeneds") || "[]");
				}).catch(error => this.$message.error(error.message));
		},
		methods: {
			menuSelect(key, path) { menuSelect(this, key, path); },
			menuChange(key, path) { menuChange(this, key, path); },
		}
	});

function fileSelect(ctx) {
	var status = {};
	status["length"] = 0;
	if (ctx.fileList != null) {
		for (var i = 0; i < ctx.fileList.length; i++) {
			status[String(i)] = {percent: 0, isFin: false, isSuccess: false};
		}
		status["length"] = ctx.fileList.length;
	}
	ctx.status = status;
	if (typeof(ctx.change) != "undefined" && ctx.change != null) {
		ctx.change(ctx.fileList, ctx.status);
	}
}
Vue.component("myupload", {
		props: ["multiple", "change"],
		template:
			"<div>" +
			"	<div tabindex=\"0\" class=\"el-upload el-upload--text\" @click=\"boxClick\" @dragenter=\"boxDragenter\" @dragover=\"boxDragover\" @dragleave=\"boxDragleave\" @drop.prevent=\"boxDrop\">" +
			"		<div class=\"el-upload-dragger\" :class=\"{'is-dragover' : isDragover}\">" +
			"			<slot>" +
			"				<i class=\"el-icon-upload\"></i>" +
			"				<div class=\"el-upload__text\">Drop files or <em>Click here</em>.</div>" +
			"			</slot>" +
			"		</div>" +
			"		<input type=\"file\" name=\"file\" ref=\"_fbtn\" :multiple=\"multiple\" @change=\"fileChange\" class=\"el-upload__input\">" +
			"	</div>" +
			"	<ul class=\"myupload-list\">" +
			"		<li v-for=\"(val, key) in fileList\" class=\"myupload-list_item\" :class=\"{'is-success' : status[key].isSuccess}\" :title=\"val.name\">"+
			"			<div class=\"myupload-progress-bar\">"+
			"				<div class=\"myupload-progress-bar_outer\">"+
			"					<div class=\"myupload-progress-bar_inner\" :style=\"'width:' + status[key].percent + '%'\"></div>"+
			"				</div>"+
			"				<div class=\"myupload-list_item_name\">"+
			"					<div class=\"icon el-icon-document\"></div>{{val.name}}"+
			"				</div>"+
			"			</div>"+
			"			<div v-if=\"status[key].isFin\" class=\"icon\" :class=\"{'el-icon-success':status[key].isSuccess, 'el-icon-error':!status[key].isSuccess}\"></div>"+
			"		</li>"+
			"	</ul>" +
			"</div>",
		data: function (){
			return {
				isDragover: false,
				fileList: null,
				status: null
			};
		},
		methods: {
			boxClick: function() {
				this.$refs["_fbtn"].click()
			},
			boxDragenter: function(evt) {
				evt.preventDefault();
				this.isDragover = true;
			},
			boxDragover: function(evt) {
				evt.preventDefault();
			},
			boxDragleave: function(evt) {
				evt.preventDefault();
				this.isDragover = false;
			},
			boxDrop: function(evt) {
				this.isDragover = false;
				if (typeof(this.multiple) == "undefined" || !this.multiple) {
					this.fileList = [evt.dataTransfer.files[0]];
				} else {
					this.fileList = evt.dataTransfer.files;
				}
				fileSelect(this);
			},
			fileChange: function(evt) {
				if (typeof(this.multiple) == "undefined" || !this.multiple) {
					this.fileList = [evt.target.files[0]];
				} else {
					this.fileList = evt.target.files;
				}
				fileSelect(this);
			}
		}
	});
function menuSelect(ctx, key, path) {
	/*var locs = key.match(/item-([-\d+]+)/)[1].split("-");
	var loc = {"children":ctx.menuData};
	var breadcrumb = [];
	while (locs.length > 0) {
		var i = parseInt(locs.shift());
		loc = loc["children"][i];
		breadcrumb.push(loc["label"])
	}*/
	//console.log(breadcrumb)
	//console.log(loc["url"])
	document.location.href = key;
}
function menuChange(ctx, key, path) {
	window.localStorage.setItem("menuOpeneds", JSON.stringify(ctx.menuOpeneds));
}

function timeFormat(timestamp){
	var time = new Date()
	time.setTime(timestamp * 1000);
	var buf = String(time.getFullYear()) + "/";
	if(time.getMonth()+1 < 10){
		buf += "0";
	}
	buf += String(time.getMonth()+1)+"/";
	if(time.getDate() < 10){
		buf += "0";
	}
	buf += String(time.getDate())+" ";
	if(time.getHours() < 10){
		buf += "0";
	}
	buf += String(time.getHours())+":";
	if(time.getMinutes() < 10){
		buf += "0";
	}
	buf += String(time.getMinutes())+":";
	if(time.getSeconds() < 10){
		buf += "0";
	}
	buf += String(time.getSeconds());
	return buf;
}

var g_heartbeat_timer;
function heartbeatData(resp) {
	var jsonData = resp.data;
	if(jsonData["state"] == "offline"){
		window.top.document.location.href = "/signin.html";
	}else if(jsonData["state"] == "online"){
	}
}
function send_heartbeat(){
	axios.get("/cgi/heartbeat.json?v="+String(Math.random()).split(".")[1]).then(heartbeatData).catch(e => {});
}
g_heartbeat_timer = setInterval("send_heartbeat()", 5000);
