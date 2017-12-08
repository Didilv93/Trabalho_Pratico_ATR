#include "historiador.h"
//#include "Windows.h"

using namespace std;
using namespace boost::interprocess;

mutex file; //mutex usado para proteger arquivo


char* itoa( int value, char* result, int base ) {
	
	// check that the base if valid
	
	if (base < 2 || base > 16) { *result = 0; return result; }	
	char* out = result;
	int quotient = value;
	do {
		*out = "0123456789abcdef"[ std::abs( quotient % base ) ];
		++out;
		quotient /= base;
	} while ( quotient );
		
	// Only apply negative sign for base 10
	
	if ( value < 0 && base == 10) *out++ = '-';
	std::reverse( result, out );
	*out = 0;
	return result;
	
}

void espera(){

while(1);

}
/************************************INICIALIZA POSITION_T***********************************/

void PreenchePositiont(position_t &Dados){
	Dados.header.id = 12;//IMEI
	strcpy(Dados.data.timestamp, "DDMMAAAAHHMMSS");
	Dados.data.latitude = 13;
	Dados.data.longitude = 14;
	Dados.data.speed = 15;
}

/********************************************************************************************/

/*************************GUARDA OS DADOS FORNECIDOS PELO GATEWAY***************************/

void GatewayHistoriador(position_t Dados){
	
	FILE *arquivo; //arquivo que guardar� os dados
	char nome[3]; //Variavel para guardar o nome do arquivo
	itoa(Dados.header.id, nome, 10); //converte o nome do arquivo para char 
	file.lock();
	arquivo = fopen(nome, "a"); //abre arquivo para escrita no final
	if (arquivo == NULL){
		cout<< "Houve um erro ao abrir o arquivo" << endl;
	}
	fwrite(&Dados, sizeof(Dados), 1, arquivo); //escreve o struct no arquivo em formato binario
	fwrite("\n", 1,1,arquivo); //insere quebra de linha

	cout << "Localizcao salva" << endl;
	fclose(arquivo);
	file.unlock();

}

/*******************************************************************************************/

/*******************************PROCURA A RESPOSTA PARA O SERVAPP****************************/

historical_data_reply_t RespHist_Serapp(historical_data_request_t pedido){
	
	historical_data_reply_t resposta; //resposta para o servidor de aplica��o
	FILE *arquivo; //arquivo que ser� aberto
	char nomearq[3]; //nome do arquivo
	itoa(pedido.id, nomearq, 10); //converte o id p/o nome do arquivo (em char) 
	file.lock();
	arquivo = fopen(nomearq, "r"); //abre arquivo para leitura
	if (arquivo == NULL){
		cout << "Houve um erro ao abrir o arquivo!" << endl;
	}
	//aloca ponteiro "num_samples" posicoes acima do final
	if (fseek(arquivo, pedido.num_samples*sizeof(resposta.data[0]), SEEK_SET) == 0)
	{
		fread(resposta.data, sizeof(resposta.data), 1, arquivo);
		resposta.num_samples_available = pedido.num_samples;
	}
	else
	{
		int i = 1;
		while (fseek(arquivo, -i*sizeof(resposta.data[0]), SEEK_END) == 0)
		{
			i++;
		}
		fseek(arquivo, - (i - 1)*sizeof(resposta.data[0]), SEEK_END);
		fread(resposta.data, sizeof(resposta.data), 1, arquivo);
		resposta.num_samples_available = i-1;
	}
	cout << "Resposta encontrada!!" << endl;
	fclose(arquivo);
	file.unlock();
	return resposta;
}

/********************************************************************************************/

/**********************************CRIA AS MSGS QUEUE**************************************/

void CriaMsgsQueue(position_t pos){
	try{
		
		message_queue::remove("gateway_historiador"); //remove message queue
		message_queue gateway_historiador
		(create_only        //only create
		,"gateway_historiador"  //name
		,100                       //max message number
		,sizeof(position_t)               //max message size
        );
		
	}

	catch(interprocess_exception &ex){
	
		std::cout << ex.what() << std::endl;

	}
	try{
		
		message_queue::remove("servapp_historiador"); //remove message queue
		message_queue servapp_historiador
		(create_only        //only create
		,"servapp_historiador"  //name
		,100                       //max message number
		,sizeof(historical_data_request_t)               //max message size
		);
	}
	catch(interprocess_exception &ex){
	
		std::cout << ex.what() << std::endl;

	}

	try{
		
		message_queue::remove("historiador_servapp"); //remove message queue
		message_queue servapp_historiador
		(create_only        //only create
		,"historiador_servapp"  //name
		,100                       //max message number
		,sizeof(historical_data_reply_t)               //max message size
        );

	}

	catch(interprocess_exception &ex){
	
		std::cout << ex.what() << std::endl;

	}

}

/******************************************************************************************/

/***********************************PEGA DADOS COM O GATEWAY******************************/

void ComunicacaoGateWay_Historiador(position_t pos){

	while(1){
		try{
		
			message_queue gateway_historiador
			(open_only        //only create
				 ,"gateway_historiador"  //name
				);

			unsigned int priority;
			message_queue::size_type recvd_size;
			cout << "Historiador: esperando receber dados do gateway" <<endl;

			gateway_historiador.receive(&pos, sizeof(pos), recvd_size, priority);
			cout << "Historiador: Dado recebido" <<endl;
			std::cout <<"Id Unico: "<<pos.header.id << std::endl;
		
		}

		catch(interprocess_exception &ex){
	
			std::cout << ex.what() << std::endl;

		}
		GatewayHistoriador(pos);
	}

}

/*****************************************************************************************/

/****************************ENVIA RESPOSTA PARA O SERVAPP*******************************/

void EnviaResposta(historical_data_reply_t resposta){

	try
   {
     
	  //Create a message_queue.
      message_queue historiador_servapp
         (open_only               //only create
         ,"historiador_servapp"           //name
         );

     historiador_servapp.send(&resposta, sizeof(resposta), 0); 
	 std::cout << "Historiador: Dados enviados para o servidor de aplicacao" << std::endl;

   }
   catch(interprocess_exception &ex){
      std::cout << ex.what() << std::endl;
   }

}

/*****************************************************************************************/

/****************************PEGA REQUISI��O DO SERV_APP**********************************/

void Requisicao(historical_data_request_t requisicao){
	while(1){
		
		try{
		
			message_queue servapp_historiador
			(open_only        //only create
			,"servapp_historiador"  //name
			 );

			unsigned int priority;
			message_queue::size_type recvd_size;
			cout << "Historiador: esperando receber requisicao do servidor de aplicacao" <<endl;

			servapp_historiador.receive(&requisicao, sizeof(requisicao), recvd_size, priority);
			cout << "Historiador: requisicao recebida" <<endl;
			std::cout <<"Id Unico: "<<requisicao.id << std::endl;
			historical_data_reply_t resposta = RespHist_Serapp(requisicao); //procura a resposta
			EnviaResposta(resposta); //envia a resposta requisitada
		
		}

		catch(interprocess_exception &ex){
	
			std::cout << ex.what() << std::endl;

		}

	}
	
 }

/****************************************************************************************/

