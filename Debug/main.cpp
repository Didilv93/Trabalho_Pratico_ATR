
#include "historiador.h"

int main(){

	
	position_t Dados; 
	PreenchePositiont(Dados); //timestampo no formato DDMMAAAAHHMMSS
	CriaMsgsQueue(Dados);
	historical_data_request_t pedido;
	std::thread p(espera);
	std::thread s(ComunicacaoGateWay_Historiador, Dados);
	std::thread t(Requisicao, pedido); // chamada de thread de tratamentos			
	p.join();
	system("pause");
	return 0;
}
