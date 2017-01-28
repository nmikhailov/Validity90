# Packets

## Init sequence

## Key exchange

1. Go to 0696, copy 48 bytes as ECSIGNATURE
2. Go to 0062, copy 70 bytes as DECRYPT1
3. 

## Encrypted packet format

| Offset | Size | Name | Description |
|---|---|---|---|
| 0000  | 0003 | header | Header, always 170303 |
| 0003  | 0002 | $length | Packet length, can span across multiple USB_BULK packets |
| 0005  | $length | data | Data, encrypted with AES256 CBC mode, PKCS#5 padding |

# Appendix A: Static keys

## KEY_EC_DEV_PUB
```
0000 45 43 53 31 20 00 00 00 f7 27 65 3b 4e 16 ce 06 
0010 65 a6 89 4d 7f 3a 30 d7 d0 a0 be 31 0d 12 92 a7 
0020 43 67 1f df 69 f6 a8 d3 a8 55 38 f8 b6 be c5 0d 
0030 6e ef 8b d5 f4 d0 7a 88 62 43 c5 8b 23 93 94 8d 
0040 f7 61 a8 47 21 a6 ca 94
```

## KEY_EC_DRV_PUB
```
0000 45 43 4b 31 20 00 00 00 5f 71 17 6f 76 66 55 74 
0010 a3 86 53 53 10 f6 98 18 6f 42 9b f0 6e fa 05 9b 
0020 0c 3f 99 bc fe b5 d6 ce 3e 61 55 91 ab 00 99 b0 
0030 4f 6f 4b 68 ac bd 67 81 65 b8 26 75 1d 50 e3 87 
0040 d0 cc fd 49 5f f4 ce ca 
```

## KEY_EC_DRV_PRIV
```
0000 45 43 53 32 20 00 00 00 00 8c ae 25 6e 6e f5 2f 
0010 50 1e 70 19 1a 21 b8 13 2e 90 ec 77 f4 5d 2d 9d 
0020 93 29 25 74 ba fd 9d ab 8f 8c 94 12 4f c3 bf aa 
0030 a5 ae 76 6a c2 18 fa 1e ef a8 bd ae 9f f1 d1 79 
0040 e0 e4 7a f0 74 04 f4 dd d6 87 4c 06 b8 2d d1 ce 
0050 dc 99 f9 2f b7 86 6c 20 09 72 23 68 b9 04 1f fa 
0060 ff 72 de 8e 49 52 f5 94 
```

## KEY_AES_MASTER

```
0000 08 02 00 00 10 66 00 00 20 00 00 00 48 78 02 70 
0010 5e 5a c4 a9 93 1c 44 aa 4d 32 25 22 39 e0 bf 8f 
0020 0c 85 4d de 49 0c cc f6 87 ef ad 9c 
```
