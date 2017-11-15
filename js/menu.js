$(document).ready(
function(){

	$('#menu > li > ul')
		.hide()
		.click(function(e){
			e.stopPropagation();
		});
	$('#menu > li').toggle(
	function()
	{
		$(this).find('ul').slideDown();
		$(this).text();
	},
	function()
	{
		$(this).find('ul').slideUp();
		$(this).text();
	});
});
