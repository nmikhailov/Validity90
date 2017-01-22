from Crypto import Random
from Crypto.Cipher import AES
import base64


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

data = bytearray.fromhex("d0d352deff4a42cd039842531a1d8962ae5b30ced3052b575ab782ac4a14cbae1461d63525f057e13f3c3b459eec327026788cb781bdda48d299f7586c234a40")


key = bytes(k1)
iv = bytes(data[:16])

cypher = AES.new(key, AES.MODE_CBC, iv)
print(cypher.decrypt(bytes(data[16:])).hex())