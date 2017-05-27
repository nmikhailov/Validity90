from Crypto import Random
from Crypto.Cipher import AES
import base64
import hexdump


BLOCK_SIZE=32

def encrypt(message, passphrase):
    # passphrase MUST be 16, 24 or 32 bytes long, how can I do that ?
    IV = Random.new().read(BLOCK_SIZE)
    aes = AES.new(passphrase, AES.MODE_CFB, IV)
    return base64.b64encode(aes.encrypt(message))

#def decrypt(enc ):
    #enc = base64.b64decode(enc)
    #iv = enc[:16]
    #cipher = AES.new(self.key, AES.MODE_CBC, iv )
    #return unpad(cipher.decrypt( enc[16:] ))


k1 = bytearray.fromhex("6A56DB3E80F6DC0B")
k2 = bytearray.fromhex("74D6B42A391F3E05")
k3 = bytearray.fromhex("F51A60DA31DBC8C7")
k4 = bytearray.fromhex("BE40D4A499F13C68")

k1.reverse()
k2.reverse()
k3.reverse()
k4.reverse()

k1.extend(k2)
k1.extend(k3)
k1.extend(k4)

#data = bytearray.fromhex("d0d352deff4a42cd039842531a1d8962ae5b30ced3052b575ab782ac4a14cbae1461d63525f057e13f3c3b459eec327026788cb781bdda48d299f7586c234a40")


data = """
00000000: 40 71 BA 90 15 98 D0 1D  D4 47 27 56 59 D0 42 5A  @q.......G'VY.BZ
00000010: 68 02 3E A9 E0 AC 46 EC  EF D2 1B 7A E2 62 DD 77  h.>...F....z.b.w
00000020: ED 78 4D F6 3A DE 82 04  C2 B2 73 4A 01 16 42 37  .xM.:.....sJ..B7
00000030: 4A 51 AC 69 84 E0 6E AC  31 B8 04 41 F8 78 F4 13  JQ.i..n.1..A.x..
00000040: 9E 67 53 BC 6C 82 9F AB  49 D8 7F 64 00 98 3F 58  .gS.l...I..d..?X
00000050: 21 40 65 48 6D DB 6C 49  85 47 70 7E 94 29 C4 B8  !@eHm.lI.Gp~.)..
00000060: 57 79 00 2E B7 87 C9 C1  65 D7 CF 72 D8 7F 7D 4E  Wy......e..r..}N
"""

data ="""
0000: 33 5c ed 15 55 3c f5 f4 de 14 a0 f2 59 68 00 a2 
0010: a0 98 58 c2 06 67 d5 c1 06 e3 bf e6 6a ec 6a c0 
0020: 2d b2 d8 77 d9 0e c4 12 e3 ab 48 ab aa b4 b9 56 
0030: 75 30 69 9d 0a c3 d9 bb ff de 42 11 bd 34 03 21 
0040: cf a2 8d 3c 1b e4 ba f0 1f f4 40 69 6f b4 78 18 
0050: f3 2d 6b 22 80 86 64 31 14 34 2a 81 2c cc d7 c6 
0060: 62 f3 9e 5f 78 a6 39 d3 db 57 c3 30 d4 dd 12 8f 
"""

data = hexdump.restore(data)

#key = bytes(k1)
key = hexdump.restore("""
48 78 02 70 
5e 5a c4 a9 93 1c 44 aa 4d 32 25 22 39 e0 bf 8f 
0c 85 4d de 49 0c cc f6 87 ef ad 9c
""")
iv = bytes(data[:16])

cypher = AES.new(key, AES.MODE_CBC, iv)

print("key", key.hex())
print("data", data.hex())
print(cypher.decrypt(bytes(data[16:])).hex())
