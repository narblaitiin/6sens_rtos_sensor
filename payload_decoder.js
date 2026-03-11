function decodeUplink(input)
{
    // input payload is an array of bytes (e.g., input.bytes)
    var bytes = input.bytes;

    // check payload length (minimum 14 bytes needed here -> VTH samples)
    if (bytes.length == 14) {
        // decode the uint64 timestamp (big-endian representation)
        var unixTimestamp = (bytes[0] << 56 >>> 0) | (bytes[1] << 48) | (bytes[2] << 40) | (bytes[3] << 32) | (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7]; // use `>>> 0` to ensure unsigned shift

        // convert and display Unix timestamp as a human-readable date and time 
        var date = new Date(unixTimestamp * 1000); // convert seconds to milliseconds
        var readable_date = date.toISOString(); // ISO format (YYYY-MM-DDTHH:mm:ss.sssZ)

        // decode the int16 values (big-endian representation)
        var battery = (bytes[8] << 8) | bytes[9];
        var temperature = (bytes[10] << 8) | bytes[11];
        var humidity = (bytes[12] << 8) | bytes[13];     

        // convert to signed 16-bit integers
        if (battery & 0x8000) battery -= 0x10000;
        if (temperature & 0x8000) temperature -= 0x10000;
        if (humidity & 0x8000) humidity -= 0x10000;

        // return decoded values as JSON
        return {
            data: {
                Timestamp: readable_date,   // timestamp (human-readable format)
                Battery: battery,           // battery level as int16
                Temperature: temperature,   // temperature (adjust as needed)
                Humidity: humidity,         // humidity (adjust as needed)               
            },
        }
    }
    // check payload length (more than 14 bytes neeeded here -> velocity samples)

    else {
        // decode the uint64 timestamp (big-endian representation)
        unixTimestamp = (bytes[0] << 56 >>> 0) | (bytes[1] << 48) | (bytes[2] << 40) | (bytes[3] << 32) | (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7]; // use `>>> 0` to ensure unsigned shift

         // convert and display Unix timestamp as a human-readable date and time 
        date = new Date(unixTimestamp * 1000); // convert seconds to milliseconds
        readable_date = date.toISOString(); // ISO format (YYYY-MM-DDTHH:mm:ss.sssZ)

       // decode the int16 values (big-endian representation)
        var amp_raw   = (input.bytes[8] << 8) | input.bytes[9];
        var ratio_raw = (input.bytes[10] << 8) | input.bytes[11];

        if (amp_raw & 0x8000) amp_raw -= 0x10000;
        if (ratio_raw & 0x8000) ratio_raw -= 0x10000;

        // return decoded values as JSON
        return {
            data: {
                Timestamp: readable_date,   // timestamp (human-readable format)
                //Timestamp: date,              // timestamp (unix format)
                Amplitude: amp_raw,
                Ratio: ratio_raw
            }
        };
    }
}