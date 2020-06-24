var mqtt_pass_changed = false;
var arguments = {};


function save_config() 
{
    console.log("save_config");
    $custom_conn = $("input[name='custom_connection']:checked").val();
    
    console.log( $custom_conn );

    var data = {};
    data.use_mqtt = ($custom_conn == 'use_mqtt') ? true : false; 
    data.mqtt_server = $("#mqtt_server").val();
    data.mqtt_prefix = $("#mqtt_prefix").val();
    data.mqtt_user = $("#mqtt_user").val();
    
    if (mqtt_pass_changed) 
    {
        data.mqtt_pass = $("#mqtt_pass").val();
    }

    data.use_http = ($custom_conn == 'use_http') ? true : false; 
    data.http_url = $("#http_url").val();

    mqtt_port = parseInt($("#mqtt_port").val())

    if (mqtt_port == NaN) 
    {
        mqtt_port = 0;
    }

    data.mqtt_port = mqtt_port;
    data.use_filtering = $("input[name='filtering']:checked").val();

    //data.coordinates = $("#coordinates").val();  // Removed from v1

    console.log( data );

    $.ajax({
        url: '/ruuvi.json',
        dataType: 'json',
        contentType: 'application/json',
        method: 'POST',
        cache: false,
        data: JSON.stringify(data)
    });
}

function get_config() 
{
    $.getJSON("/ruuvi.json", function(data) {
        if (data != null) 
        {
            for (var key in data) 
            {
                var el = $("#" + key);

                if (el.length > 0) 
                {
                    if (el[0].type == "checkbox") 
                    {
                        el[0].checked = data[key];
                    } 
                    else 
                    {
                        $("#" + key).val(data[key]);
                    }
                }
            }
        }
    });
}

function showError(error) 
{
    switch (error.code) 
    {
        case error.PERMISSION_DENIED:
            msg = "Error: Geolocation not allowed."
            console.log(msg)
            alert(msg)
            break;
        case error.POSITION_UNAVAILABLE:
            msg = "Location information is unavailable."
            console.log(msg)
            alert(msg)
            break;
        case error.TIMEOUT:
            msg = "The request to get user location timed out."
            console.log(msg)
            alert(msg)
            break;
        case error.UNKNOWN_ERROR:
            msg = "An unknown error occurred."
            console.log(msg)
            alert(msg)
            break;
    }
}


$(document).ready(function() 
{
    //get configuration from flash and fill the web page
    get_config();

    $("#save_config").on("click", function() {
        save_config();
    });

    $("#mqtt_pass").on("change", function() {
        mqtt_pass_changed = true;
    });
});