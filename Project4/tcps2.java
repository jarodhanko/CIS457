import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.Scanner;
import java.util.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import java.security.*;
import java.security.spec.*;
import javax.xml.bind.DatatypeConverter;


class ClientSocketChannel{
	public SocketChannel sc;
	public int id;
    public SecretKey secKey;
	public IvParameterSpec iv;
	
	public ClientSocketChannel(SocketChannel sc, int id){
		this.sc = sc;
		this.id = id;
	}

	public ClientSocketChannel(SocketChannel sc, int id, PublicKey pubKey){
		this(sc, id);
		this.secKey = secKey;
	}
}

class Tcps2{
	private static ArrayList<ClientSocketChannel> clients = new ArrayList<ClientSocketChannel>();
	private static int id = 0;
	public static volatile boolean quit = false;
	private static PrivateKey privKey;
    private static PublicKey pubKey;

	public static void main(String args[]){
		try{
			ServerSocketChannel c = ServerSocketChannel.open();
			Scanner scanner = new Scanner(System.in);
			System.out.println("Enter a Port: ");
			int i = scanner.nextInt();
			c.bind(new InetSocketAddress(i));
						
			privKey = CryptoHelpers.loadPrivateKey("RSApriv.der");
			pubKey = CryptoHelpers.loadPublicKey("RSApub.der");
			
			while(!quit){
				SocketChannel sc = c.accept();				
				ClientSocketChannel csc = new ClientSocketChannel(sc, id++);
				clients.add(csc); //id of client is index in arraylist
				//TcpsThreadOut out = new TcpsThreadOut(sc);
				//out.start();
				TcpsThreadIn in = new TcpsThreadIn(csc);
				in.start();
			}
			System.exit(0);
		}catch(IOException e){
			System.out.println("Got an IO Exception");
		}
	}

	public static ArrayList<ClientSocketChannel> getClients(){
		return clients;
	}

	public static void removeClient(int id){
		for(ClientSocketChannel c : clients){
			if (c.id == id){
				clients.remove(c);
				break;
			}
		}
	}
	
	public static void kill(int id){ 
		//kill client
	}

	public static PublicKey getPubKey(){
		return pubKey;
	}
	public static PrivateKey getPrivKey(){
		return privKey;
	}

	public static IvParameterSpec generateIv(ClientSocketChannel csc){		
		SecureRandom r = new SecureRandom();
		byte ivbytes[] = new byte[16];
		r.nextBytes(ivbytes);
		csc.iv = new IvParameterSpec(ivbytes);
		return csc.iv;
	}
}

class TcpsThreadOut extends Thread{
	private ClientSocketChannel csc;
	private ClientMessage message;

	public TcpsThreadOut(ClientSocketChannel csc, ClientMessage message){
		this.csc = csc;
		this.message = message;
	}


	public void run(){		
		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		ObjectOutput out = null;
		try{
			System.out.println("Sending messsage to client #" + csc.id);
			out = new ObjectOutputStream(bos);
			out.writeObject(this.message);
			out.flush();
			byte messageBytes[] = bos.toByteArray();
			byte encryptedBytes[] = null;
			if(csc.secKey != null){
				encryptedBytes = CryptoHelpers.encrypt(messageBytes, csc.secKey, Tcps2.generateIv(csc));
			}else{
				encryptedBytes = messageBytes;
			}
			ClientMessageWithIv cmiv = new ClientMessageWithIv(encryptedBytes, csc.iv.getIV());
			ByteArrayOutputStream bos2 = new ByteArrayOutputStream();
			ObjectOutput out2 = null;
			try {
				out2 = new ObjectOutputStream(bos2);
				out2.writeObject(cmiv);
				out2.flush();
			}finally{
				try{
					bos2.close();
				}catch(IOException ex){}
			}
			ByteBuffer buf = ByteBuffer.wrap(bos2.toByteArray());
			csc.sc.write(buf);
			if (message.getType() == MessageType.KICK){				
				csc.sc.close();
				Tcps2.removeClient(csc.id);
			}
		}catch(IOException ex){
			if (message.getType() == MessageType.KICK){		
				Tcps2.removeClient(csc.id);
			}
			//we quit the application or there was an error
		}finally{
			try{
				bos.close();
			}catch(IOException e){}
		}
	}
}

class TcpsThreadIn extends Thread{
	private ClientSocketChannel csc;
	private volatile boolean runLoop = true;

	public TcpsThreadIn(ClientSocketChannel csc){
		this.csc = csc;
	}

	public void run(){
		ObjectInput kin = null;
		try{
			//save secretkey and ivparams
			ByteBuffer keyBuf = ByteBuffer.allocate(256);
			int length = csc.sc.read(keyBuf);
			byte[] keyArry = new byte[length];
			System.arraycopy(keyBuf.array(), 0, keyArry, 0, length);
			byte[] kserializedBuf = CryptoHelpers.RSADecrypt(keyArry, Tcps2.getPrivKey());
			ByteArrayInputStream kbis = new ByteArrayInputStream(kserializedBuf);
			KeyExchange kcm = null;

			kin = new ObjectInputStream(kbis);
			kcm = (KeyExchange)kin.readObject();
			csc.secKey = new SecretKeySpec(kcm.getKey(),"AES");		
				
		}catch(Exception ex){
			System.out.println("Error: " + ex.getStackTrace());			
		}finally{
			try{
				if(kin != null){
					kin.close();
				}
			}catch(IOException e){}	
		}
		while(runLoop){
			try{
				ByteBuffer buf = ByteBuffer.allocate(4096);							
				int length = csc.sc.read(buf);
				byte[] shortArry = new byte[length];
				System.arraycopy(buf.array(), 0, shortArry, 0, length);
				byte[] serializedBuf = null;				
				ByteArrayInputStream bis2 = new ByteArrayInputStream(shortArry);
				ObjectInput in2 = null;
				ClientMessageWithIv cmiv = null;
				IvParameterSpec iv = null;
				try{
					in2 = new ObjectInputStream(bis2);
					cmiv = (ClientMessageWithIv)in2.readObject();
				}catch(ClassNotFoundException ex){
					System.out.println("Client Message is corrupted");
					return;
				}finally{
					try{
						if(in2 != null){
							in2.close();
						}
					}catch(IOException e){}	
				}
				iv = new IvParameterSpec(cmiv.iv);
				if(csc.secKey != null){
					serializedBuf = CryptoHelpers.decrypt(cmiv.c, csc.secKey, iv);
				}else{
					serializedBuf = buf.array();
				}
				handleIncomingMessage(serializedBuf, csc.id);
			}catch(Exception ex){
				runLoop = false;	
				Tcps2.removeClient(csc.id);
			}
		}
	}

	public void handleIncomingMessage(byte[] buf, int id) throws IOException{
		System.out.println("handling");
		ByteArrayInputStream bis = new ByteArrayInputStream(buf);
		ObjectInput in = null;
		ClientMessage cm = null;
		try{
			in = new ObjectInputStream(bis);
			cm = (ClientMessage)in.readObject();
		}catch(ClassNotFoundException ex){
			System.out.println("Client Message is corrupted");
			return;
		}finally{
			try{
				if(in != null){
					in.close();
				}
			}catch(IOException e){}	
		}
		String message = cm.getMessage();		
		if(message.trim().length() > 0){
			System.out.println(message);
		}
		if(message.trim().toLowerCase().equals("quit")){
			Tcps2.kill(-1);
			csc.sc.close();
			runLoop = false;
			throw new IOException("Kill process");
		}
		ArrayList<ClientSocketChannel> clients = Tcps2.getClients();
		switch (cm.getType()){
			case MESSAGE:
				for (ClientSocketChannel c : clients){
					if (c.id == cm.getClientId()){
						System.out.println("starting thread 1");
						cm.setClientId(id);
						TcpsThreadOut out = new TcpsThreadOut(c, cm);
						out.start();
						break;
					}
				}
				break;
			case BROADCAST:
				for (ClientSocketChannel c : clients){
					//if (c.id != cm.getClientId()){
					cm.setClientId(id);
					TcpsThreadOut out = new TcpsThreadOut(c, cm);
					out.start();
					System.out.println("starting thread 2");
					//}
				}
				break;
			case KICK:
				for (ClientSocketChannel c : clients){
					if(c.id == cm.getClientId()){
						System.out.println("Kicking Client #" + cm.getClientId());
						TcpsThreadOut out = new TcpsThreadOut(c, cm);
						out.start();
					}
				}
				break;
			case LIST:
				String clientList = "The client Ids are listed below: \n";
				for(ClientSocketChannel c : clients){
					clientList += c.id + "\n";
				}
				cm.setMessage(clientList);
				System.out.println(cm.getMessage());
				for (ClientSocketChannel c : clients){
					if (c.id == id){
						TcpsThreadOut out = new TcpsThreadOut(c, cm);
						out.start();
					}
				}
				break;
			case ADMIN:
				//not implemented yet, simply there incase it is needed later
				break;
		}
	}
}

enum MessageType {
	MESSAGE,
	BROADCAST,
	KICK,
	LIST,
	ADMIN
}

class KeyExchange implements Serializable{
	private static final long serialVersionUID = 1L;
	private byte[] key;
	
	public KeyExchange(){}
	public KeyExchange(byte[] key){
		this.key = key;
	}

	public byte[] getKey(){
		return key;
	}
}

class ClientMessageWithIv implements Serializable {
	private static final long serialVersionUID = 1L;
	public byte[] c;
	public byte[] iv;

	public ClientMessageWithIv(byte[] c, byte[] iv){
		this.c = c;
		this.iv = iv;
	}
} 


class ClientMessage implements Serializable {
	private static final long serialVersionUID = 1L;
	private	MessageType type;
	private int clientId;
	private String message;

	public ClientMessage(){}
	public ClientMessage(MessageType type, int clientId, String message){
		this.type = type;
		this.clientId = clientId;
		this.message = message;
	}

	/* Getters and Setters */
	public MessageType getType() { return this.type; }
	public void setType(MessageType type) { this.type = type; }
	public int getClientId() { return this.clientId; }
	public void setClientId(int clientId) { this.clientId = clientId; }
	public String getMessage() { return this.message; }
	public void setMessage(String message) { this.message = message; }
}
