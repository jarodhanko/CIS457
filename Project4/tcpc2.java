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

	public static IvParameterSpec generateIv(){		
		SecureRandom r = new SecureRandom();
		byte ivbytes[] = new byte[16];
		r.nextBytes(ivbytes);
		iv = new IvParameterSpec(ivbytes);
		return iv;
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
		KeyExchange keyMessage = new KeyExchange(Tcpc2.getS().getEncoded()); //, Tcpc2.getIv().getIV());
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
						if(Tcpc2.getS() != null){
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.generateIv());
						}else{
							encryptedBytes = messageBytes;
						}
						ClientMessageWithIv cmiv = new ClientMessageWithIv(encryptedBytes, Tcpc2.getIv().getIV());
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
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.generateIv());
						else
							encryptedBytes = messageBytes;
						
						ClientMessageWithIv cmiv = new ClientMessageWithIv(encryptedBytes, Tcpc2.getIv().getIV());
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
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.generateIv());
						else
							encryptedBytes = messageBytes;
						
						ClientMessageWithIv cmiv = new ClientMessageWithIv(encryptedBytes, Tcpc2.getIv().getIV());
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
							encryptedBytes = CryptoHelpers.encrypt(messageBytes, Tcpc2.getS(), Tcpc2.generateIv());
						else
							encryptedBytes = messageBytes;				
						
						ClientMessageWithIv cmiv = new ClientMessageWithIv(encryptedBytes, Tcpc2.getIv().getIV());
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

				byte decryptedBytes[] = null;				
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

				if(Tcpc2.getS() != null){
					decryptedBytes = CryptoHelpers.decrypt(cmiv.c, Tcpc2.getS(), iv);
				}else{
					decryptedBytes = shortArry;
				}
				
				ByteArrayInputStream bis = new ByteArrayInputStream(decryptedBytes);
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
			}catch(Exception ex){
				runLoop = false;
				Tcpc2.kill();
				try{
					sc.close();
				}catch(Exception e){}
			}
		}
	}
}
