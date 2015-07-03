var xhrRequest = function (url, type, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        callback(this.responseText);
    };
    xhr.open(type, url);
    xhr.send();
};

function locationSuccess(pos) {
    // Construct URL
    var url = "http://api.openweathermap.org/data/2.5/weather?lat=" +
        pos.coords.latitude + "&lon=" + pos.coords.longitude;

    // Send request to OpenWeatherMap
    xhrRequest(url, 'GET',
               function(responseText) {
                   // responseText contains a JSON object with weather info
                   var json = JSON.parse(responseText);

                   // Temperature in Kelvin requires adjustment
                   var temperature = Math.round(json.main.temp - 273.15);

                   // Assemble dictionary using our keys
                   var dictionary = {
                       "KEY_TEMPERATURE": temperature
                   };

                   // Send to Pebble
                   Pebble.sendAppMessage(dictionary);
               }
              );
}

function locationError(err) {
}

function getWeather() {
    navigator.geolocation.getCurrentPosition(
        locationSuccess,
        locationError,
        {timeout: 15000, maximumAge: 60000}
    );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', function(e) {
    console.log("PebbleKit JS ready!");
    getWeather();
} );

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage', function(e) {
    // TODO: Check what kind of message!
    console.log("AppMessage received!");
    getWeather();
} );
