#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <mutex>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <iostream>
#include <vector>
#include <thread>

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

static const unsigned MAX_POSITION_SAMPLES = 30;

struct position_t{
	
	struct{
		int id;
	} header;
	
	struct{
		char timestamp[16];
		double latitude;
		double longitude;
		int speed;
	} data;

};


struct historical_data_request_t{
	int id;
	int num_samples;
};

struct historical_data_reply_t{
	int num_samples_available;
	position_t data[MAX_POSITION_SAMPLES];
};

void GatewayHistoriador(position_t Dados);
historical_data_reply_t RespHist_Serapp(historical_data_request_t pedido);

void PreenchePositiont(position_t &Dados);
void CriaPedidoSerapp_Hist(historical_data_request_t &pedido);
void Requisicao(historical_data_request_t requisicao);
void EnviaResposta(historical_data_reply_t resposta);
void ComunicacaoGateWay_Historiador(position_t pos);
void CriaMsgsQueue(position_t pos);
void espera();
