import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.Scanner;
import java.util.*;


class ClientSocketChannel{
	public SocketChannel sc;
	public int id;

	public ClientSocketChannel(SocketChannel sc, int id){
		this.sc = sc;
		this.id = id;
	}
}

class Tcps{
	private static ArrayList<ClientSocketChannel> clients = new ArrayList<ClientSocketChannel>();
	private static int id = 0;
	public static volatile boolean quit = false;
	public static void main(String args[]){
		try{
			ServerSocketChannel c = ServerSocketChannel.open();
			Scanner scanner = new Scanner(System.in);
			System.out.println("Enter a Port: ");
			int i = scanner.nextInt();
			c.bind(new InetSocketAddress(i));
			
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
			ByteBuffer buf = ByteBuffer.wrap(messageBytes);
			csc.sc.write(buf);
			if (message.getType() == MessageType.KICK){				
				csc.sc.close();
				Tcps.removeClient(csc.id);
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
		while(runLoop){
			try{
				ByteBuffer buf = ByteBuffer.allocate(4096);
				csc.sc.read(buf);
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
				if(message.trim().toLowerCase().equals("quit")){
					Tcps.kill(-1);
					csc.sc.close();
					runLoop = false;
					break;
				}
				if(message.trim().length() > 0){
					System.out.println(message);
				}
				handleIncomingMessage(cm, csc.id);
			}catch(IOException ex){
				runLoop = false;
			}catch(ClassNotFoundException ex){
				runLoop=false;
			}
		}
	}

	public void handleIncomingMessage(ClientMessage cm, int id) throws IOException{
		System.out.println("handling");
		ArrayList<ClientSocketChannel> clients = Tcps.getClients();
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
