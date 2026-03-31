function decodeUplink(input) {
  var bytes = input.bytes;

  // ── Helpers ──────────────────────────────────────────────────────────────
function decodeTimestamp(b, offset) {
  const buffer = new Uint8ClampedArray(b);
  // We wrap the buffer in a DataView
  const view = new DataView(buffer.buffer,  0, b.byteLength);
  
  // getBigUint64(offset, littleEndian)
  // The 'true' argument specifies Little-Endian
  return Number(view.getBigUint64(offset, true));
}

  function int16(hi, lo) {
    var v = (lo << 8) | hi;
    if (v & 0x8000) v -= 0x10000;
    return v;
  }

  // ── Frame structure ───────────────────────────────────────────────────────
  // Byte 0    : ID
  // Bytes 1–8 : uint32 Unix timestamp (little-endian)
  // Bytes 9+  : payload (depends on ID)

  // Minimum check: at least ID + timestamp
  if (bytes.length < 5) {
    return { errors: ["Payload too short (min 5 bytes: 1 ID + 4 timestamp)"] };
  }

  var id     = bytes[0];
  var unixTs = decodeTimestamp(bytes, 1);  // bytes 1-4

  // ── Dispatch on ID (length is no longer used to distinguish cases) ─────────
  switch (id) {

    // ── ID 1 : VTH sample — bat(2) + temp(2) + hum(2) = 6 bytes → total 11 ──
    case 1: {
      if (bytes.length !== 15) {
        return { errors: ["ID 1 expects exactly 11 bytes, got " + bytes.length] };
      }
      return {
        data: {
          ID          : id,
          Timestamp   : unixTs,
          Battery     : int16(bytes[9], bytes[10]),
          Temperature : (int16(bytes[11], bytes[12]))/100,
          Humidity    : (int16(bytes[13], bytes[14]))/100,
        }
      };
    }

    // ── ID 2 : Velocity sample — amp(2) + ratio(2) = 4 bytes → total 9 ───────
    // ── ID 2 : Velocity sample — min(2) + max(2) + ratio(2) → total 11 bytes ─
    case 2: {
      if (bytes.length !== 15) {
        return { errors: ["ID 2 expects exactly 11 bytes, got " + bytes.length] };
      }
      return {
        data: {
          ID        : id,
          Timestamp : unixTs,
          MinSTA       : int16(bytes[9], bytes[10]),
          MaxSTA       : int16(bytes[11], bytes[12]),
          STALTA    : (int16(bytes[13], bytes[14]))/100,
        }
      };
    }

       // ── ID 3 : Samples ─────────────────────────────────────────────────
    case 3: {
      if (bytes.length < 11) {            // 1 + 8 + 2
        return { errors: ["ID 3 payload too short (need 11 bytes)"] };
      }
      let payload_bytes = bytes.length - 11
      if (payload_bytes % 2 !== 0) {            // 1 + 8 + 2
        return { errors: ["ID 3 Error, payload has an even number of bytes, but samples are coded on two bytes"] };
      }

      samples = []
      for(let i = 9; i < bytes.length; i +=2)  {
        samples.push(int16(bytes[i], bytes[i+1]))
      }

      return {
        data: {
          ID        : id,
          Timestamp : unixTs,
          Samples : samples
        }
      };
    }

    // ── ID 4 : Periodic Sample ─────────────────────────────────────────────────
    case 4: {
      if (bytes.length !== 15) {            // 1 + 4 + 4 = 9
        return { errors: ["ID 2 payload too short (need 9 bytes)"] };
      }
      return {
        data: {
          ID        : id,
          Timestamp : unixTs,
          MinSTA : int16(bytes[9], bytes[10]), 
          MaxSTA : int16(bytes[11], bytes[12]),
          MeanSTA: int16(bytes[13], bytes[14]),
        }
      };
    }

    default:
      return { errors: ["Unknown ID: " + id] };
  }
}