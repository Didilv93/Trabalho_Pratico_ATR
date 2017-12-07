// ***********************************************************************************
// *				    Módulo GATEWAY                                   *
// *                     Automação Em Tempo Real - Trabalho Final                    *
// *                                                                                 *
// *			               						     *
// *		         Diego Santos Silva 	   - 2014056972                      *
// *                     Ângelo 		   - 2009025894                      *
// *										     *
// *										     *		
// *	              Professor André Paim Lemos - 2017/2		             *
// *                                                                                 *
// ***********************************************************************************

#include "Gateway.h"

using namespace boost::interprocess;
using boost::asio::ip::tcp;
using namespace std;

static const int MAX_LENGTH = 1024;
int c = -1;
int nClient = -1; // primeiro cliente ocupa a posição 0
position_t usuario[5000];
active_users_t* ativo;
mutex m;

int atualizaUsuariosAtivos(position_t usuario)
{
	//Insere ou edita usuarios ativos		
	bool edicao = false;

	for (int j = 0; j <= nClient; j++)
	{
		//Edita dados caso usuario ja esta conectado
		if (ativo->list[j].id == usuario.id && ativo->list[j].id != -1) {
			edicao = true;

			ativo->list[j].timestamp = usuario.timestamp;
			ativo->list[j].latitude = usuario.latitude;
			ativo->list[j].longitude = usuario.longitude;
			ativo->list[j].speed = usuario.speed;
			return j;
		}
	}
	if (edicao == false) { //Insere dados do cliente ativo

		nClient++;
		ativo->num_active_users = nClient;
		ativo->list[nClient].id = usuario.id;
		ativo->list[nClient].timestamp = usuario.timestamp;
		ativo->list[nClient].latitude = usuario.latitude;
		ativo->list[nClient].longitude = usuario.longitude;
		ativo->list[nClient].speed = usuario.speed;
		return nClient;
	}
	return 0;
}

void retiraUsuarioDaLista(position_t usuario)
{
	int achou = 0;
	for (int j = 0; j < nClient; j++)
	{
		// Retira o usuáerio da lista puxando todos os usuários "para o inicio" em 1 posição
		if ((ativo->list[j].id == usuario.id && ativo->list[j].id != -1)|| achou == 1)
		{
			ativo->list[j] = ativo->list[j - 1];
			achou = 1;
		}
	}
	if (achou == 1)
	{
		nClient--;
		c--;
		ativo->num_active_users = nClient;
	}
	return;
}

void enviaPosicaoDoUsuario(position_t usuario) // Funcao que implementa fila de mensagens entre Gateway e Historiador
{
	message_queue mq(open_only, "gateway_historiador");
	mq.send(&usuario, sizeof(usuario), 0);
}

class session
	: public std::enable_shared_from_this<session>
{
public:
	session(tcp::socket socket)
		: socket_(std::move(socket))
	{
	}

	void start()
	{
		do_read();
	}

private:
	void do_read()
	{
		auto self(shared_from_this());
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			[this, self](boost::system::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				// Separando os itens da mensagem
				string hold[17];  //Serão lidos 17 tokens
				int countTokens = 0;
				char *next_token = NULL;
				char * pch;
				pch = strtok_r(data_, "?", &next_token); //Separacao GET /
				hold[countTokens] = pch;
				countTokens++;
				while ((pch != NULL) && (countTokens < 17)) // Cria um vetor com as strings de cada token
				{
					pch = strtok_r(NULL, "=", &next_token);
					hold[countTokens] = pch;
					countTokens++;
					if (countTokens != 16)
					{
						pch = strtok_r(NULL, "&", &next_token);
					}
					else
						pch = strtok_r(NULL, " ", &next_token);
					hold[countTokens] = pch;
					countTokens++;
				}
				// Compara o inicio da mensagem 

//				if (hold[0] == "GET /")
				if (hold[0] == "POST /")
				{
					m.lock();
					c++;
					//Completa dados de posicao do usuario
					usuario[c].id = std::stoi(hold[2]);
					usuario[c].timestamp = std::stoi(hold[4]);
					cout << "             Recebi dados do client com ID:" << hold[2] << " Timestamp: " << hold[4] << endl;
					cout << "Dados:\n             Latitude:" << hold[6] <<
		 			              "\n             Longitude:" << hold[8] <<
					              "\n             Velocidade:" << hold[10] << "\n" << endl;
					usuario[c].latitude = std::stod(hold[6]);
					usuario[c].longitude = std::stod(hold[8]);
					usuario[c].speed = std::stoi(hold[10]);

					atualizaUsuariosAtivos(usuario[c]);	//Atualiza lista de usuarios ativos (Shared Mem)
					enviaPosicaoDoUsuario(usuario[c]);				//Envia informacoes para o historiador
					m.unlock();
				}
				loop();
			}
			else
			{
				cout << "             Cliente desconectado: " << socket_.remote_endpoint() << endl;
				//retiraUsuarioDaLista(usuario[c]);
				//cout << "             Usuario com ID: " << usuario[c].id << " retirado da lista de usuarios ativos" << endl;
			}
		});
	}

	void loop()
	{
			do_read();
	}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
};

class server
{
public:
	server(boost::asio::io_service& io_service, short port)
		: acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
		socket_(io_service)
	{
		do_accept();
	}

private:
	void do_accept()
	{
		acceptor_.async_accept(socket_,
			[this](boost::system::error_code ec)
		{
			if (!ec)
			{
				std::make_shared<session>(std::move(socket_))->start();
			}

			do_accept();
		});
	}
	tcp::acceptor acceptor_;
	tcp::socket socket_;
};

int main(int argc, char* argv[])
{
	cout << "                                              GATEWAY                                            " << endl;
	cout << "                                          Dezembro de 2017                                         " << endl;
	cout << endl;
	cout << "                                Aguardando conexoes na porta: 9001                               \n" << endl;
	cout << endl;
	shared_memory_object::remove("UsuariosAtivos");
	shared_memory_object shm(open_or_create, "UsuariosAtivos", read_write);
	shm.truncate(1000);
	mapped_region UsuariosAtivos(shm, read_write);
	cout << "             Memoria compartilhada com o servidor -> CRIADA E MAPEADA\n";
	ativo = static_cast<active_users_t*>(UsuariosAtivos.get_address());
	try
	{
		boost::asio::io_service io_service;
		server s(io_service, std::atoi("9001"));
		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
