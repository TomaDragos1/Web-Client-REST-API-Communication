Toma Mihai Dragos 322CB


First i did some strucutres that helped me:
tcp message when i send and receive form the clinet
clinet that is a structre that help me keep track of all the clinets (i have a list with clients)
subscirption topic that has all the topic that the udp client send me, and has a list of subscribers
subscriber that i use to keep track of each client that subed to a topic and here i work with the sf


server:
    first i did the tcp and udp sockets and multiplexed them with the poll from the c library
    in the while o iterate to all the sockets and which of them got an even
    STDIN:
        just the exit command the closes all the sockes
    UDP:
        here i read the message from buffe and i create all the categories
        i put all the message needed in a string that i will send to the client
        after that i create te sub if its the first one, if not i am tryng to send the mesage the the users:
            firs i itreate to all the sub_topics until i find the right one
            after that i iterate throuhg all the subs of that topic
                every sub i search it in my client list because there i have a queue to send sf messages and there i see if the user is online or not
                if its online i just send the message 
                if not i will push in my queue the string(message to clinet) that every client in my list has

    TCP:
        here first from the clinet, when the ./subscriber is runned i will send the id to the server. the server will look if the clinet exits in my list
        if it does i will look if its online or not
            it its online i will send an error message bcs i cannot connect two users with the same id
            it its offline i will pop all the elements in the queue and send the message form the sf option to the user
            after that i will associate a new socket for the user if the user is valid

    USERS:
        afte that all the is left are the sockets used for clinet-server comunication
        here i used the tcp strucure to help me communicate between them
        first if the clinet types exit i will return a strucrre with a type 0 (exit) or the receiv will return 0 byts
        if that happend i close the socket and then i got it out of the poll of socket
        type 1 is subscribe. i just make a subscriber and put him in the topic given by the tcp struct parameter with the sf
        type 2 is unsubscribe. same thing but i eras the subscriber

    client:
        i make 2 sockets: tcp and stdin
        for stdin i have 3 commands: exit(i send type zero), subscribe and unsubscribe
        for the las two i just parse the buffer and fill the tcp fields
        tcp: here i recieve the string and i just print int
        if its an error i exit

    i alose deacivated the nagle alorithm and made the send with fragmentation for tcp
