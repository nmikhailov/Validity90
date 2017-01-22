package.path = package.path .. ";./lockbox/?.lua"

local String = require("string");

local Array = require("lockbox.util.array");
local Stream = require("lockbox.util.stream");

local CBCMode = require("lockbox.cipher.mode.cbc");

local PKCS7Padding = require("lockbox.padding.pkcs7");
local ZeroPadding = require("lockbox.padding.zero");

local AES256Cipher = require("lockbox.cipher.aes256");


validy90_proto = Proto("validy90", "Validy 90 fingerprint reader protocol")

local f = validy90_proto.fields
f.f_magic_header = ProtoField.uint24("validy90.magic", "Magic Header", base.HEX)
f.f_length = ProtoField.uint16("validy90.length", "Packet length bytes", base.DEC)
f.f_iv = ProtoField.bytes("validy90.iv", "Encryption AES IV")
f.f_data = ProtoField.bytes("validy90.data", "Encrypted data")
f.f_dec_data = ProtoField.bytes("validy90.dec_data", "Decrypted data")
f.f_particial = ProtoField.bool("validy90.partial", "Is partial", base.NONE, {[0] = "no", [1] = "yes"})

local f_direction = Field.new("usb.endpoint_number.direction")

local CONST_MAGIC_HEADER = ByteArray.new("170303")

local packetDb = {}
local partialBuffer = nil

local function decode_aes(ivStr, dataStr)
    -- body
    local decipher = CBCMode.Decipher()
    
    local key = nil
    local dir = f_direction()
    
    if tostring(f_direction()) == "1" then
        key = Array.fromHex(validy90_proto.prefs["aes_in"])
    else
        key = Array.fromHex(validy90_proto.prefs["aes_out"])
    end

    local iv = Array.fromHex(ivStr)
    local ciphertext = Array.fromHex(dataStr)
    decipher
            .setKey(key)
            .setBlockCipher(AES256Cipher)
            .setPadding(PKCS7Padding);

    local plainOutput = decipher
                        .init()
                        .update(Stream.fromArray(iv))
                        .update(Stream.fromArray(ciphertext))
                        .finish()
                        .asHex();

    return plainOutput
end

function validy90_proto.dissector(buffer, pinfo, tree)
    local buf = nil

    if packetDb[pinfo.number] == nil or packetDb[pinfo.number].buf == nil then
        packetDb[pinfo.number] = {};

        if partialBuffer == nil then
            partialBuffer = buffer:bytes()
        else
            partialBuffer:append(buffer:bytes())
        end

        buf = partialBuffer:tvb("Joint packet")
    else
        local state = packetDb[pinfo.number]

        buf = state.buf:tvb("Joint packet")
    end

	pinfo.cols["protocol"] = "Validy90"

	-- create protocol tree
	local t_validy90 = tree:add(validy90_proto, buf())
	local offset = 0

    -- Header
    local magic_header = buf(offset, 3)
    offset = offset + 3
    if magic_header:bytes() == CONST_MAGIC_HEADER then
        t_validy90:add(f.f_magic_header, magic_header)

        -- Len
        local len = buf(offset, 2)
        offset = offset + 2
        t_validy90:add(f.f_length, len)
        
        if buf:len() - 5 < len:uint() then
            pinfo.cols["info"]:append(string.format(" INCOMPLETE %d left", len:uint() - buf:len() + 5))
            t_validy90:add(f.f_particial, true)
        elseif buf:len() - 5 == len:uint() then
            pinfo.cols["info"]:append(string.format(" COMPLETED", len:uint() - buf:len() + 5))
            partialBuffer = nil
            t_validy90:add(f.f_particial, 0)

            -- iv
            local iv = buf(offset, 16)
            offset = offset + 16
            t_validy90:add(f.f_iv, iv)

            -- Raw Data
            local data = buf(offset)
            t_validy90:add(f.f_data, data)

            -- Decode
            local dec_data = ByteArray.new(decode_aes(iv:bytes():tohex(), data:bytes():tohex())):tvb("Decrypted")
            t_validy90:add(f.f_dec_data, dec_data())
        else
            pinfo.cols["info"]:append(string.format(" INVALID", len:uint() - buf:len() + 5))
        end
    else
        t_validy90:add(f.f_magic_header, magic_header)
        pinfo.cols["info"]:append(string.format(" Invalid header %#03x", magic_header:le_uint()))
        partialBuffer = nil
    end

    packetDb[pinfo.number].buf = buf:bytes()
end


-- preferences
validy90_proto.prefs["aes_in"] = Pref.string("IN AES Key", "", "")
validy90_proto.prefs["aes_out"] = Pref.string("OUT AES Key", "", "")


usb_table = DissectorTable.get("usb.bulk")
usb_table:add(0xFF, validy90_proto)
usb_table:add(0xFFFF, validy90_proto)
