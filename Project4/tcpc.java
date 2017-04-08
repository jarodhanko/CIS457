import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.Scanner;

class Tcpc {
	public static volatile boolean quit = false;
	public static void main(String args[]){
		try{
			SocketChannel sc = SocketChannel.open();
			Scanner scanner = new Scanner(System.in);
			System.out.println("Enter an IP address: "); //"127.0.0.1"
			String s = scanner.nextLine();
			System.out.println("Enter a Port: "); // 9876
			int i = scanner.nextInt();
			sc.connect(new InetSocketAddress(s, i));
			
			TcpClientThreadOut out = new TcpClientThreadOut(sc);
			TcpClientThreadIn in = new TcpClientThreadIn(sc);
			out.start();
			System.out.println("Please choose one of the following actions: \n" + 
						"\t Message - sends a message to a single client \n" +
						"\t Broadcast - sends a message to all the clients \n" + 
						"\t Kick - kick a client \n" +
						"\t List - list all the clients \n" +
						"\t Help - repeat this message");
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
}


class TcpClientThreadOut extends Thread{
	private SocketChannel sc;
	private volatile boolean runLoop = true;

	TcpClientThreadOut(SocketChannel sc){
		this.sc = sc;
	}

	public void run(){
		Scanner scanner = new Scanner(System.in);
		Console cons = System.console();
		while(runLoop){
			try{				

				String m = cons.readLine("");
				if(m.trim().toLowerCase().equals("quit")){
					ByteBuffer buf = ByteBuffer.wrap("quit".getBytes());
					sc.write(buf);
					tcpclientchat.kill();
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
						ByteBuffer buf = ByteBuffer.wrap(messageBytes);
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
						ByteBuffer buf = ByteBuffer.wrap(messageBytes);
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
						ByteBuffer buf = ByteBuffer.wrap(messageBytes);
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
						ByteBuffer buf = ByteBuffer.wrap(messageBytes);
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
	SocketChannel sc;
	private volatile boolean runLoop = true;

	TcpClientThreadIn(SocketChannel sc){
		this.sc = sc;
	}

	public void run(){
		Console cons = System.console();
		while(runLoop){
			try{
				ByteBuffer buf = ByteBuffer.allocate(4096);
				sc.read(buf);
				System.out.println("received something");
				ByteArrayInputStream bis = new ByteArrayInputStream(buf.array());
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
						Tcpc.kill();
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
