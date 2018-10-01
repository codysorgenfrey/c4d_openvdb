var pages = [
	["Home", "OpenVDB for Cinema 4D", "index.html"],
	["VDB Primitive Object", "VDB Primitive Object", "OC4DOPENVDBPRIMITIVE.html"],
	["VDB Visualizer Object", "VDB Visualizer Object", "OC4DOPENVDBVISUALIZER.html"]
];
var thispage = [];

$( document ).ready(function(){
	$(pages).each(function(){
		if (this[2] == curFile){
			thispage = this;
			$("ul.uk-nav").append("<li class='uk-active'><a href='"+this[2]+"'>"+this[0]+"<a/></li>");
		} else {
			$("ul.uk-nav").append("<li><a href='"+this[2]+"'>"+this[0]+"<a/></li>");
		}
	});
	$("title, #mainTitle").html(thispage[1]);
	$("#top").append("<li><a href='"+thispage[2]+"'>"+thispage[0]+"<a/></li>");
	$(".section").each(function(){
		$("#inpage").append("<li><a href='#"+$(this).attr("id")+"'>"+$(this).html()+"<a/></li>");
	});
});