validy90_proto = Proto("validy90", "Validy 90 fingerprint reader protocol")

local f = validy90_proto.fields
f.f_magic_header = ProtoField.uint24("validy90.magic", "Magic Header", base.HEX)
f.f_length = ProtoField.uint16("validy90.length", "Packet length bytes", base.DEC)
f.f_iv = ProtoField.bytes("validy90.iv", "Encryption AES IV")
f.f_data = ProtoField.bytes("validy90.data", "data")
f.f_particial = ProtoField.bool("validy90.partial", "Is partial", base.NONE, {[0] = "no", [1] = "yes"})

local CONST_MAGIC_HEADER = ByteArray.new("170303")

local packetDb = {}
local partialBuffer = nil

function validy90_proto.dissector(buffer, pinfo, tree)
    local buf = nil

    if packetDb[pinfo.number] == nil then
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
