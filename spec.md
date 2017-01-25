# Packets

## Init sequence

## Key exchange

## Encrypted packet format

| Offset | Size | Name | Description |
|---|---|---|---|
| 0000  | 0003 | header | Header, always 170303 |
| 0003  | 0002 | $length | Packet length, can span across multiple USB_BULK packets |
| 0005  | $length | data | Data, encrypted with AES256 CBC mode, PKCS#5 padding |
