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
	public	IvParameterSpec iv;
	
	public ClientSocketChannel(SocketChannel sc, int id){
		this.sc = sc;
		this.id = id;
	}

	public ClientSocketChannel(SocketChannel sc, int id, PublicKey pubKey, IvParameterSpec iv){
		this(sc, id);
		this.secKey = secKey;
		this.iv = iv;
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
}

class TcpsThreadOut extends Thread{
	private ClientSocketChannel csc;
	private ClientMessage message;

	public TcpsThreadOut(ClientSocketChannel csc, ClientMessage message){
		this.csc = csc;
		this.message = message;
	}


	public void run(){		
		System.out.println("starting");
		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		ObjectOutput out = null;
		try{
			System.out.println("attempting to send");
			out = new ObjectOutputStream(bos);
			out.writeObject(this.message);
			out.flush();
			byte messageBytes[] = bos.toByteArray();
			byte encryptedBytes[] = null;
			if(csc.secKey != null && csc.iv != null){
				encryptedBytes = CryptoHelpers.encrypt(messageBytes, csc.secKey, csc.iv);
			}else{
				encryptedBytes = messageBytes;
			}
			ByteBuffer buf = ByteBuffer.wrap(encryptedBytes);
			csc.sc.write(buf);
			if (message.getType() == MessageType.KICK){				
				csc.sc.close();
				Tcps2.removeClient(csc.id);
			}
		}catch(IOException ex){
			System.out.println("Exception: " + ex.getStackTrace());
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
			csc.iv = new IvParameterSpec(kcm.getIv());
				
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
				if(csc.secKey != null && csc.iv != null){
					serializedBuf = CryptoHelpers.decrypt(shortArry, csc.secKey, csc.iv);
				}else{
					serializedBuf = buf.array();
				}
				handleIncomingMessage(serializedBuf, csc.id);
			}catch(IOException ex){
				runLoop = false;
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
					System.out.println("starting thread 1");
					if (c.id == cm.getClientId()){
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
	private byte[] key;
	private byte[] iv;
	
	public KeyExchange(){}
	public KeyExchange(byte[] key, byte[] iv){
		this.key = key;
		this.iv = iv;	
	}

	public byte[] getKey(){
		return key;
	}
	public byte[] getIv(){
		return iv;
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
