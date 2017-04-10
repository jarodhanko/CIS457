import java.io.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import java.security.*;
import java.security.spec.*;
import javax.xml.bind.DatatypeConverter;

public class CryptoHelpers{
	public static byte[] encrypt(byte[] plaintext, SecretKey secKey, IvParameterSpec iv){
		try{
			Cipher c = Cipher.getInstance("AES/CBC/PKCS5Padding");
			c.init(Cipher.ENCRYPT_MODE,secKey,iv);
			byte[] ciphertext = c.doFinal(plaintext);
			return ciphertext;
		}catch(Exception e){
			System.out.println(plaintext.length);
			System.out.println(e.getMessage());
			System.out.println("AES Encrypt Exception");
			System.exit(1);
			return null;
		}
    }
    public static byte[] decrypt(byte[] ciphertext, SecretKey secKey, IvParameterSpec iv){
		try{
			Cipher c = Cipher.getInstance("AES/CBC/PKCS5Padding");
			c.init(Cipher.DECRYPT_MODE,secKey,iv);
			byte[] plaintext = c.doFinal(ciphertext);
			return plaintext;
		}catch(Exception e){
			System.out.println(ciphertext.length);
			System.out.println(e.getMessage());
			System.out.println("AES Decrypt Exception");
			System.exit(1);
			return null;
		}
    }
    public static byte[] RSADecrypt(byte[] ciphertext, PrivateKey privKey){
		try{
			Cipher c = Cipher.getInstance("RSA/ECB/OAEPWithSHA-1AndMGF1Padding");
			c.init(Cipher.DECRYPT_MODE,privKey);
			byte[] plaintext=c.doFinal(ciphertext);
			return plaintext;
		}catch(Exception e){
			System.out.println(ciphertext.length);
			System.out.println(e.getMessage());
			System.out.println("RSA Decrypt Exception");
			System.exit(1);
			return null;
		}
    }
    public static byte[] RSAEncrypt(byte[] plaintext, PublicKey pubKey){
		try{
			System.out.println(plaintext.length);
			System.out.println(pubKey.getEncoded().length);
			Cipher c = Cipher.getInstance("RSA/ECB/OAEPWithSHA-1AndMGF1Padding");
			c.init(Cipher.ENCRYPT_MODE,pubKey);
			byte[] ciphertext=c.doFinal(plaintext);
			return ciphertext;
		}catch(Exception e){
			System.out.println(plaintext.length);
			System.out.println(e.getMessage());
			System.out.println("RSA Encrypt Exception");
			System.exit(1);
			return null;
		}
    }
    public static SecretKey generateAESKey(){
		try{
			KeyGenerator keyGen = KeyGenerator.getInstance("AES");
			keyGen.init(128);
			SecretKey secKey = keyGen.generateKey();
			return secKey;
		}catch(Exception e){
			System.out.println("Key Generation Exception");
			System.exit(1);
			return null;
		}
    }
    public static PrivateKey loadPrivateKey(String filename){
		try{
			File f = new File(filename);
			FileInputStream fs = new FileInputStream(f);
			byte[] keybytes = new byte[(int)f.length()];
			fs.read(keybytes);
			fs.close();
			PKCS8EncodedKeySpec keyspec = new PKCS8EncodedKeySpec(keybytes);
			KeyFactory rsafactory = KeyFactory.getInstance("RSA");
			PrivateKey privKey = rsafactory.generatePrivate(keyspec);
			return privKey;
		}catch(Exception e){
			System.out.println("Private Key Exception");
			e.printStackTrace(System.out);
			System.exit(1);
		}
		return null;
    }
    public static PublicKey loadPublicKey(String filename){
		try{
			File f = new File(filename);
			FileInputStream fs = new FileInputStream(f);
			byte[] keybytes = new byte[(int)f.length()];
			fs.read(keybytes);
			fs.close();
			X509EncodedKeySpec keyspec = new X509EncodedKeySpec(keybytes);
			KeyFactory rsafactory = KeyFactory.getInstance("RSA");
			PublicKey pubKey = rsafactory.generatePublic(keyspec);
			return pubKey;
		}catch(Exception e){
			System.out.println("Public Key Exception");
			System.exit(1);
		}
		return null;
    }
}
