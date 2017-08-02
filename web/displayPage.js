

function showInfo(evt) {
	$('.contentContainer').html ( "<p>This is the info tab</p>");
	tablinks = document.getElementsByClassName("tab");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
	document.getElementById("info").style.display = "block";
    evt.currentTarget.className += " active";
}

function showShows(evt) {
	$('.contentContainer').html ( "<p>This is the show tab</p>");
	tablinks = document.getElementsByClassName("tab");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
	document.getElementById("shows").style.display = "block";
    evt.currentTarget.className += " active";
}