

// First, checks if it isn't implemented yet.
if (!String.prototype.format) {
  String.prototype.format = function() {
    var args = arguments;
    return this.replace(/{(\d+)}/g, function(match, number) { 
      return typeof args[number] != 'undefined'
        ? args[number]
        : match
      ;
    });
  };
}


var apList = null;
var selectedSSID = "";
var refreshAPInterval = null; 
var checkStatusInterval = null;


function stopCheckStatusInterval()
{
	if(checkStatusInterval != null)
	{
		clearInterval(checkStatusInterval);
		checkStatusInterval = null;
	}
}

function stopRefreshAPInterval()
{
	if(refreshAPInterval != null)
	{
		clearInterval(refreshAPInterval);
		refreshAPInterval = null;
	}
}

function startCheckStatusInterval()
{
	checkStatusInterval = setInterval(checkStatus, 950);
}

function startRefreshAPInterval()
{
	refreshAPInterval = setInterval(refreshAP, 2800);
}


// Check if fields are filled and at least 8 characters to enable join button
function check_wifi_fields_filled()
{
    // disable join button by default
    $('.connection-join').attr('disabled', 'disabled');

    $('.connection-field').keyup(function() 
    {
        var empty = false;

        $('.connection-field').each(function() 
        {
            if ($(this).val() == '' || $(this).val().length < 8) 
            {
                empty = true;
            }
        });

        if (empty) 
        {
            $('.connection-join').attr('disabled', 'disabled');
        } 
        else 
        {
            $('.connection-join').removeAttr('disabled'); 
        }
    });
}

function check_manual_wifi_fields_filled()
{
    // disable button by default
    $('.manual-connection-join').attr('disabled', 'disabled');

    $('#manual_pwd').keyup(function() 
    {
        var empty = false;
        $('#manual_pwd').each(function() 
        {
            if ($(this).val() == '' || $(this).val().length < 8) {
                empty = true;
            }
        });

        if (empty) 
        {
            $('.manual-connection-join').attr('disabled', 'disabled');
        } 
        else 
        {
            $('.manual-connection-join').removeAttr('disabled'); 
        }
    });
}



// The popstate event is fired each time when the current history entry changes.
window.addEventListener('popstate', function(event) 
{
    var url = window.location.hash.substring(1);

   	$('section.section').hide();

	$('#'+url).show();     

}, false);



$(document).ready(function()
{

	// Set initial hash to help back button navigation
	window.location.hash = 'welcome';	

	// Wifi password field checks
	check_wifi_fields_filled();

	check_manual_wifi_fields_filled();


	// Language switcher 
	$(".lang_select").change(function() 
	{
	    var lang = $(this).val();

	    $("[lang]").each(function () 
	    {
	        if ($(this).attr("lang") == lang) $(this).fadeIn();
	        else $(this).hide();
	    });
	});
    
	// Navigation
	function change_url(url)
	{
		$('section.section').hide();
		$('#'+url).show();

		window.location.hash = url;
	}


	$("#use_http").change(function() 
	{
	    if(this.checked) 
	    {
	       $('.conf-settings-http').slideDown();
	       $('.conf-settings-mqtt').slideUp();
	       $('#use_mqtt').prop('checked', false);
	    }
	    else
	    {
	    	$('.conf-settings-http').slideUp();
	    }
	});

	$("#use_mqtt").change(function() 
	{
	    if(this.checked) 
	    {
	       $('.conf-settings-mqtt').slideDown();
	       $('.conf-settings-http').slideUp();
	       $('#use_http').prop('checked', false);
	    }
	    else
	    {
	    	$('.conf-settings-mqtt').slideUp();
	    }
	});

	$('.btn-navi').click(function(e)
	{
		e.preventDefault();
		
		var target_hash = $(this).data('target');

		switch (target_hash)
		{
			case 'back':
				window.history.back(-1);
			break;

			case 'settings-custom':
				var connection_type = $("input[name='connection_type']:checked").val();

				if (connection_type == 'ruuvi') change_url('wifi');
				else change_url('settings-custom');
			break;

			case 'connect':
				var network_type = $("input[name='network_type']:checked").val();

				if (network_type == 'wifi') change_url('wifi-list');
				else change_url('cable-settings');
			break;

			case 'confirm':
				$('.connect-wifi-name').hide();
				$('.connect-cable').show();

				change_url('thankyou');
				save_config();
			break;	

			default: 
				change_url(target_hash);	
		} 

	});


    $("#temp").on("click", function()
    {
        $('section.section').hide();
    	$('#wifi-overlay').removeClass('loading');
		$('#wifi-overlay').fadeOut();
		$('#manual-overlay').removeClass('loading');
		$('#manual-overlay').fadeOut();
    	$('#thankyou').show();

    	save_config();
    });

   
   


    $('#show-password').click(function(e)
    {
		if ( $('#pwd').prop("type") === "password") 
		{
			$("#pwd").prop("type", "text");
		} 
		else 
		{
			$("#pwd").prop("type", "password");
		}
	});
	
	$("#wifi-status").on("click", ".ape", function() {
		$( "#wifi" ).slideUp( "fast", function() {});
		$( "#connect-details" ).slideDown( "fast", function() {});
	});

	$("#manual_add").on("click", ".ape", function() {
		selectedSSID = $(this).text();
		$( ".wifi-network-name" ).text(selectedSSID);
		$( "#wifi" ).slideUp( "fast", function() {});
		$( "#connect_manual" ).slideDown( "fast", function() {});
		$( "#connect" ).slideUp( "fast", function() {});

		//update wait screen
		$( "#loading" ).show();
		$( "#connect-success" ).hide();
		$( "#connect-fail" ).hide();
	});

	$('#use-manual-wifi').click(function(e)
	{
		e.preventDefault();
		$('#manual-overlay').fadeIn();
	});

	
	$(".btn-cancel").click(function() 
	{
		selectedSSID = "";
		$('.overlay-container').removeClass('loading');
		$('.overlay-container').hide();
	});
/*
	$("#manual_cancel").click(function() 
	{
		selectedSSID = "";
		$('#manual-overlay').removeClass('loading');
		$('#manual-overlay').hide();
	});
	*/
	$("#join").click(function() 
	{
		$('#wifi-overlay').addClass('loading');
		performConnect();
	});

	$("#manual_join").click(function() 
	{
		performConnect($(this).data('connect'));
	});
	
	$("#ok-details").on("click", function() {
		$( "#connect-details" ).slideUp( "fast", function() {});
		$( "#wifi" ).slideDown( "fast", function() {});
		
	});
	
	$("#ok-credits").on("click", function() {
		$( "#credits" ).slideUp( "fast", function() {});
		$( "#app" ).slideDown( "fast", function() {});
		
	});
	
	$("#acredits").on("click", function(event) {
		event.preventDefault();
		$( "#app" ).slideUp( "fast", function() {});
		$( "#credits" ).slideDown( "fast", function() {});
	});
	
	$("#ok-connect").on("click", function() {
		$( "#connect-wait" ).slideUp( "fast", function() {});
		$( "#wifi" ).slideDown( "fast", function() {});
	});
	
	$("#disconnect").on("click", function() {
		$( "#connect-details-wrap" ).addClass('blur');
		$( "#diag-disconnect" ).slideDown( "fast", function() {});
	});
	
	$("#no-disconnect").on("click", function() {
		$( "#diag-disconnect" ).slideUp( "fast", function() {});
		$( "#connect-details-wrap" ).removeClass('blur');
	});
	
	$("#yes-disconnect").on("click", function() {
		
		stopCheckStatusInterval();
		selectedSSID = "";
		
		$( "#diag-disconnect" ).slideUp( "fast", function() {});
		$( "#connect-details-wrap" ).removeClass('blur');
		
		$.ajax({
			url: '/connect.json',
			dataType: 'json',
			method: 'DELETE',
			cache: false,
			data: { 'timestamp': Date.now()}
		});

		startCheckStatusInterval();
		
		$( "#connect-details" ).slideUp( "fast", function() {});
		$( "#wifi" ).slideDown( "fast", function() {})
	});
	

	//first time the page loads: attempt get the connection status and start the wifi scan
	refreshAP();
	startCheckStatusInterval();
	startRefreshAPInterval();
	
});


// Init function needed as click events are lost everytime list is updated
function initWifiList()
{
	$('.wifi-list a').click(function(e) 
	{
    	e.preventDefault();

		selectedSSID = $(this).text();

    	var auth_needed = selected_wifi_auth_required(selectedSSID);

    	if (auth_needed)
    	{
			$('#wifi-overlay').fadeIn();
    		$('#pwd').focus();
    	}	
    	else
    	{
    		$('#no-auth-overlay').fadeIn();
    		performConnect();
    	}

		$( ".wifi-network-name" ).text(selectedSSID);
    });
}


// Check if need for auth screen 
function selected_wifi_auth_required(ssid)
{
	var selectedItem = jQuery.grep(apList, function(item) 
	{
	    return (item.ssid === ssid);
	});

	
	if (selectedItem.length) 
	{ 	
		return (selectedItem.auth == 0);
	}

	return true;
}


function performConnect(conntype)
{
	//stop the status refresh. This prevents a race condition where a status 
	//request would be refreshed with wrong ip info from a previous connection
	//and the request would automatically shows as succesful.
	stopCheckStatusInterval();
	
	//stop refreshing wifi list
	stopRefreshAPInterval();

	var pwd;
	if (conntype == 'manual') 
	{
		//Grab the manual SSID and PWD
		selectedSSID=$('#manual_ssid').val();
		pwd = $("#manual_pwd").val();
	} 
	else 
	{
		pwd = $("#pwd").val();
	}

	//reset connection 
	$( "#loading" ).show();
	$( "#connect-success" ).hide();
	$( "#connect-fail" ).hide();
	
	$( "#ok-connect" ).prop("disabled",true);
	$( ".wifi-network-name" ).text(selectedSSID);
	$( "#connect" ).slideUp( "fast", function() {});
	$( "#connect_manual" ).slideUp( "fast", function() {});
	$( "#connect-wait" ).slideDown( "fast", function() {});
	
	
	$.ajax({
		url: '/connect.json',
		dataType: 'json',
		method: 'POST',
		cache: false,
		headers: { 'X-Custom-ssid': selectedSSID, 'X-Custom-pwd': pwd },
		data: { 'timestamp': Date.now()}
	});


	//now we can re-set the intervals regardless of result
	startCheckStatusInterval();
	startRefreshAPInterval();
	
}



function rssiToIcon(rssi)
{
	if(rssi >= -60){
		return 'w0';
	}
	else if(rssi >= -67){
		return 'w1';
	}
	else if(rssi >= -75){
		return 'w2';
	}
	else{
		return 'w3';
	}
}


// Load wifi list
function refreshAP()
{
	$.getJSON( "ap.json", function( data ) 
	{
		if(data.length > 0)
		{
			//sort by signal strength
			data.sort(function (a, b) 
			{
				var x = a["rssi"]; var y = b["rssi"];
				return ((x < y) ? 1 : ((x > y) ? -1 : 0));
			});

			apList = data;

			refreshAPHTML(apList);
		}
	});
}

// Refresh wifi selection list
function refreshAPHTML(data)
{
	var h = "";
	data.forEach(function(e, idx, array) 
	{
		//h += '<div class="ape{0}"><div class="{1}"><div class="{2}">{3}</div></div></div>'.format(idx === array.length - 1?'':' brdb', rssiToIcon(e.rssi), e.auth==0?'':'pw',e.ssid);
		h += '<li><a href="" class="{0}">{3}<span class="{2}"></span></a></li>'.format(idx === array.length - 1?'':' brdb', rssiToIcon(e.rssi), e.auth==0?'':'pw',e.ssid);
		h += "\n";
	});
	
	$( "#ssid-list" ).html(h);

	initWifiList();
}


function checkStatus()
{
	$.getJSON( "/status.json", function( data ) 
	{
		if(data.hasOwnProperty('ssid') && data['ssid'] != "")
		{
			if(data["ssid"] === selectedSSID)
			{
				//that's a connection attempt
				if(data["urc"] === 0)
				{
					//got connection
					$(".wifi-network-name").text(data["ssid"]);
					//$("#connect-details h1").text(data["ssid"]);
					$("#ip").text(data["ip"]);
					$("#netmask").text(data["netmask"]);
					$("#gw").text(data["gw"]);
					$("#wifi-status").slideDown( "fast", function() {});
					
					//unlock the wait screen if needed
					$( "#ok-connect" ).prop("disabled",false);
					
					//update wait screen
					$( "#loading" ).hide();
					$( "#connect-success" ).show();
					$( "#connect-fail" ).hide();
				}
				else if(data["urc"] === 1)
				{
					//failed attempt
					$(".wifi-network-name").text('');
					$("#connect-details h1").text('');
					$("#ip").text('0.0.0.0');
					$("#netmask").text('0.0.0.0');
					$("#gw").text('0.0.0.0');
					
					//don't show any connection
					$("#wifi-status").slideUp( "fast", function() {});
					
					//unlock the wait screen
					$( "#ok-connect" ).prop("disabled",false);
					
					//update wait screen
					$( "#loading" ).hide();
					$( "#connect-fail" ).show();
					$( "#connect-success" ).hide();
				}
			}
			else if(data.hasOwnProperty('urc') && data['urc'] === 0)
			{
				//ESP32 is already connected to a wifi without having the user do anything
				if( !($("#wifi-status").is(":visible")) )
				{
					$(".wifi-network-name").text(data["ssid"]);
					$("#connect-details h1").text(data["ssid"]);
					$("#ip").text(data["ip"]);
					$("#netmask").text(data["netmask"]);
					$("#gw").text(data["gw"]);
					$("#wifi-status").slideDown( "fast", function() {});
				}
			}
		}
		else if(data.hasOwnProperty('urc') && data['urc'] === 2)
		{
			//that's a manual disconnect
			if($("#wifi-status").is(":visible"))
			{
				$("#wifi-status").slideUp( "fast", function() {});
			}
		}
	})
	.fail(function() 
	{
		//don't do anything, the server might be down while esp32 recalibrates radio
	});


}
