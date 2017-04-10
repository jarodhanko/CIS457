import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.Scanner;
import java.io.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import java.security.*;
import java.security.spec.*;
import javax.xml.bind.DatatypeConverter;

class Tcpc2 {
	public static volatile boolean quit = false;
    private static PublicKey pubKey;
	private static SecretKey s;
	private static IvParameterSpec iv;
	public static void main(String args[]){
		try{
			SocketChannel sc = SocketChannel.open();
			Scanner scanner = new Scanner(System.in);
			System.out.println("Enter an IP address: "); //"127.0.0.1"
			String str = scanner.nextLine();
			System.out.println("Enter a Port: "); // 9876
			int i = scanner.nextInt();
			sc.connect(new InetSocketAddress(str, i));

			//might actually have to receive from server, but not for now
			pubKey = CryptoHelpers.loadPublicKey("RSApub.der");

			s = CryptoHelpers.generateAESKey();
			SecureRandom r = new SecureRandom();
			byte ivbytes[] = new byte[16];
			r.nextBytes(ivbytes);
			iv = new IvParameterSpec(ivbytes);
		
			TcpClientThreadOut out = new TcpClientThreadOut(sc);
			TcpClientThreadIn in = new TcpClientThreadIn(sc);
			out.start();
			in.start();
			while(!quit){}
			System.out.println("quiting");
			sc.close();
			System.exit(0);
		}catch(IOException e){		
			System.out.println("Got an IO Exception");
		}
	}
	
	public static void kill(){ 
		quit = true; 
	}

	public static SecretKey getS(){
		return s;
	}

	public static IvParameterSpec getIv(){
		return iv;
	}
	public static PublicKey getPubKey(){
		return pubKey;
	}
}


class TcpClientThreadOut extends Thread{
	private SocketChannel sc;
	private volatile boolean runLoop = true;

	TcpClientThreadOut(SocketChannel sc){
		this.sc = sc;
	}

	public void run(){	
		//send encrypted secret key
		KeyExchange keyMessage = new KeyExchange(Tcpc2.getS().getEncoded(), Tcpc2.getIv().getIV());
		ByteArrayOutputStream kbos = new ByteArrayOutputStream();
		ObjectOutput kout = null;
		try {
			kout = new ObjectOutputStream(kbos);
			kout.writeObject(keyMessage);
			kout.flush();
			byte[] kmessageBytes = kbos.toByteArray();
			//encrypt			
			byte kencryptedBytes[] = null;
			if(Tcpc2.getPubKey() != null)
				kencryptedBytes = CryptoHelpers.RSAEncrypt(kmessageBytes, Tcpc2.getPubKey());
			else
				kencryptedBytes = kmessageBytes;
			ByteBuffer kbuf = ByteBuffer.wrap(kencryptedBytes);
			sc.write(kbuf);
		}catch(IOException ex){
			System.out.println("Exception: " + ex.getStackTrace());
			return;
		}finally{
			try{
				kbos.close();
			}catch(IOException ex){}
		}
		System.out.println("Please choose one of the following actions: \n" + 
						"\t Message - sends a message to a single client \n" +
						"\t Broadcast - sends a message to all the clients \n" + 
						"\t Kick - kick a client \n" +
						"\t List - list all the clients \n" +
						"\t Help - repeat this message");
		Scanner scanner = new Scanner(System.in);
		Console cons = System.console();
		while(runLoop){
			try{				

				String m = cons.readLine("");
				if(m.trim().toLowerCase().equals("quit")){
					ByteBuffer buf = ByteBuffer.wrap("quit".getBytes());
					sc.write(buf);
					Tcpc2.kill();
					sc.close();
					runLoop = false;
				}else if(m.trim().toLowerCase().equals("broadcast")){
					String message = cons.readLine("Please enter your message:\n");
					ClientMessage cm = new ClientMessage(MessageType.BROADCAST, -1, message.trim());
					ByteArrayOutputStream bos = new ByteArrayOutputStream();
					ObjectOutput out = null;
					try {
						out = new ObjectOutputStream(bos);
						out.writeObject(cm);
						out.flush();
						byte[] messageBytes = bos.toByteArray();
						//encrypt			
						byte encryptedBytes[] = null;
						if(Tcpc2.getS() != null)
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.getIv());
						else
							encryptedBytes = messageBytes;
						ByteBuffer buf = ByteBuffer.wrap(encryptedBytes);
						sc.write(buf);
					}finally{
						try{
							bos.close();
						}catch(IOException ex){}
					}
				}else if (m.trim().toLowerCase().equals("kick")){
					System.out.println("Please enter the id of the client you wish to kick"); //"127.0.0.1"
					int id = scanner.nextInt();
					ClientMessage cm = new ClientMessage(MessageType.KICK, id, "");
					ByteArrayOutputStream bos = new ByteArrayOutputStream();
					ObjectOutput out = null;
					try {
						out = new ObjectOutputStream(bos);
						out.writeObject(cm);
						out.flush();
						byte[] messageBytes = bos.toByteArray();
						//encrypt			
						byte encryptedBytes[] = null;
						if(Tcpc2.getS() != null)
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.getIv());
						else
							encryptedBytes = messageBytes;
						ByteBuffer buf = ByteBuffer.wrap(encryptedBytes);
						sc.write(buf);
					}finally{
						try{
							bos.close();
						}catch(IOException ex){}
					}
				}else if (m.trim().toLowerCase().equals("list")){
					ClientMessage cm = new ClientMessage(MessageType.LIST, -1, "");
					ByteArrayOutputStream bos = new ByteArrayOutputStream();
					ObjectOutput out = null;
					try {
						out = new ObjectOutputStream(bos);
						out.writeObject(cm);
						out.flush();
						byte[] messageBytes = bos.toByteArray();
						//encrypt			
						byte encryptedBytes[] = null;
						if(Tcpc2.getS() != null)
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.getIv());
						else
							encryptedBytes = messageBytes;
						ByteBuffer buf = ByteBuffer.wrap(encryptedBytes);
						sc.write(buf);
					}finally{
						try{
							bos.close();
						}catch(IOException ex){}
					}
				}else if (m.trim().toLowerCase().equals("help")){					
					System.out.println("Please choose one of the following actions: \n" + 
									"\t Message - sends a message to a single client \n" +
									"\t Broadcast - sends a message to all the clients \n" + 
									"\t Kick - kick a client \n" +
									"\t List - list all the clients \n" +
									"\t Help - repeat this message");
				}else if(m.trim().toLowerCase().equals("message")){
					System.out.println("Please enter the id of the client you wish to message"); //"127.0.0.1"
					int id = scanner.nextInt();
					String message = cons.readLine("Please enter your message:\n");
					ClientMessage cm = new ClientMessage(MessageType.MESSAGE, id, message.trim());
					ByteArrayOutputStream bos = new ByteArrayOutputStream();
					ObjectOutput out = null;
					try {
						out = new ObjectOutputStream(bos);
						out.writeObject(cm);
						out.flush();
						byte[] messageBytes = bos.toByteArray();
						//encrypt			
						byte encryptedBytes[] = null;
						if(Tcpc2.getS() != null)
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.getIv());
						else
							encryptedBytes = messageBytes;
						ByteBuffer buf = ByteBuffer.wrap(encryptedBytes);
						sc.write(buf);
					}finally{
						try{
							bos.close();
						}catch(IOException ex){}
					}
				}
			}catch(IOException ex){
				//we quit the application or there was an error
				runLoop = false;
			}
		}	
	}
}

class TcpClientThreadIn extends Thread{
	private SocketChannel sc;
	private volatile boolean runLoop = true;

	TcpClientThreadIn(SocketChannel sc){
		this.sc = sc;
	}

	public void run(){
		Console cons = System.console();
		while(runLoop){
			try{
				ByteBuffer buf = ByteBuffer.allocate(4096);
				int length = sc.read(buf);
				System.out.println("received something");				
				byte[] shortArry = new byte[length];
				System.arraycopy(buf.array(), 0, shortArry, 0, length);

				byte encryptedBytes[] = null;
				if(Tcpc2.getS() != null && Tcpc2.getIv() != null){
					encryptedBytes = CryptoHelpers.decrypt(shortArry, Tcpc2.getS(), Tcpc2.getIv());
				}else{
					encryptedBytes = shortArry;
				}
				
				ByteArrayInputStream bis = new ByteArrayInputStream(encryptedBytes);
				ObjectInput in = null;
				ClientMessage cm = null;
				try{
					in = new ObjectInputStream(bis);
					cm = (ClientMessage)in.readObject();
				}finally{
					try{
						if(in != null){
							in.close();
						}
					}catch(IOException e){}	
				}
				String message = cm.getMessage();	
				switch (cm.getType()){
					case MESSAGE:					
						if(message.trim().length() > 0){
							System.out.println("Received from client " + cm.getClientId() + ": " + message);
						}
						break;
					case BROADCAST:	
						//if(message.trim().length() > 0){
							System.out.println("Received from client " + cm.getClientId() + ": " + message);
						//}
						break;
					case LIST:
						System.out.println("Received: " + cm.getMessage());
						break;
					case KICK:
						System.out.println("You have been kicked from the server");
						Tcpc2.kill();
						sc.close();
						runLoop = false;
						break;
				}
			}catch(IOException ex){
				runLoop = false;
			}catch(ClassNotFoundException ex){
				runLoop = false;
			}
		}
	}
}
