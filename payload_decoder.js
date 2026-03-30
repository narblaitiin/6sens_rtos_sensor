function decodeUplink(input) {
  var bytes = input.bytes;

  // ── Helpers ──────────────────────────────────────────────────────────────
  function decodeTimestamp(b, offset) {
    // uint32 big-endian timestamp (Unix seconds fit in 32 bits until year 2106)
    return ((b[offset] << 24) | (b[offset+1] << 16) |
            (b[offset+2] << 8) | b[offset+3]) >>> 0;
  }

  function int16(hi, lo) {
    var v = (hi << 8) | lo;
    if (v & 0x8000) v -= 0x10000;
    return v;
  }

  // ── Frame structure ───────────────────────────────────────────────────────
  // Byte 0      : ID
  // Bytes 1–4   : uint32 Unix timestamp (big-endian)
  // Bytes 5+    : payload (depends on ID)

  if (bytes.length < 5) {
    return { errors: ["Payload too short (min 5 bytes: 1 ID + 4 timestamp)"] };
  }

  var id     = bytes[0];
  var unixTs = decodeTimestamp(bytes, 1);  // bytes 1-4

  // ── Dispatch ──────────────────────────────────────────────────────────────
  switch (id) {

    // ── ID 1 : VTH sample (battery + temperature + humidity) ────────────────
    case 1: {
      if (bytes.length < 11) {           // 1 + 4 + 6 = 11
        return { errors: ["ID 1 payload too short (need 11 bytes)"] };
      }
      return {
        data: {
          ID          : id,
          Timestamp   : unixTs,
          Battery     : int16(bytes[5], bytes[6]),
          Temperature : float(bytes[7], bytes[8])/100,
          Humidity    : float(bytes[9], bytes[10])/100,
        }
      };
    }

    // ── ID 2 : Velocity sample (amplitude + ratio) ───────────────────────────
    case 2: {
      if (bytes.length < 13) {            // 1 + 4 + 8 = 13
        return { errors: ["ID 2 payload too short (need 9 bytes)"] };
      }
      return {
        data: {
          ID        : id,
          Timestamp : unixTs,
          Amplitude : int16(bytes[5], bytes[6]),
          MinLTA    : int16(bytes[7], bytes[8]),
          MaxSTA    : int16(bytes[9], bytes[10]),
          STALTA   : int16(bytes[11], bytes[12])    
        }
      };
    }

    // ── ID 3 : Samples values ─────────────────────────────────────────────────
    case 3: {
      if (bytes.length < 13) {            // 1 + 4 + 4 = 9
        return { errors: ["ID 2 payload too short (need 9 bytes)"] };
      }
      return {
        data: {
          ID        : id,
          Timestamp : unixTs,
          Sample1 : int16(bytes[5], bytes[6]), 
          Sample2 : int16(bytes[7], bytes[8]), 
        }
      };
    }

    default:
      return { errors: ["Unknown ID: " + id] };
  }
}