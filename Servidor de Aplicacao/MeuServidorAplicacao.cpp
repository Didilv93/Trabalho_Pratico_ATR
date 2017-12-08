/* ======================================================================= */
/* =              Trabalho Final de Automação em Tempo Real              = */
/* =         Ministrado pelo professor André Paim Lemos em 2017/2        = */
/* =                                                                     = */
/* =			             Servidor de Aplicacao			             = */
/* =                                                                     = */
/* =		  		   Diego F. B. Escribano - 2013430684                = */
/* =                 Mariana Ferreira Marques - 2015122170               = */
/* =	  			                             			             = */
/* ======================================================================= */

#include "MeuServAppFuncoes.h"

//Mutex para tratar secoes criticas entre as varias conexoes
mutex m;

// Lista de usuarios ativos
position_t lista_usuarios[300];
// Apontador do struct de clientes ativos
active_users_t* cliente_ativo;

//Funcao que ira verificar e contar o numero de clientes ativos
string req_ativos()
{
	string msg, id_string;
	int i, ativos;

	//Mensagem sempre comeca com ATIVOS para essa requisicao
	msg = "ATIVOS;";

	ativos = cliente_ativo->num_active_users;
	ativos = ativos + 1;
	//String da mensagem, concatena o numero de clientes ativos
	msg = msg + to_string(ativos);
	//Se existem clientes ativos

	//lock no mutex pois agora ira ler a lista
	m.lock();
	if (ativos > 0)
	{
		// Loop para coletar o ID de cada usuario ativo
		for (i = 0; i < ativos; i++)
		{
			if (-1 != cliente_ativo->list[i].id)
			{
				// Coleta ID do usuario e insere ele duas vezes
				id_string = to_string(cliente_ativo->list[i].id);
				msg = msg + ';' + id_string + ';' + id_string;
			}
		}
	}
	//Delimitador de mensagem
	msg = msg + '\n';
	//saida da secao critica
	m.unlock();
	//retorna a mensagem
	return msg;
}

//Funcao que acessa região de memoria compartilhada com o Gateway caso o usuario
//peca somente uma amostra, ou solicita e recebe info do historiador para mais 
//amostras.
string req_hist(string id, string num_amostras)
{
	int i, posicao_lista = -1, num_amostras_int, ID_int = stoi(id);
	string msg = "HIST;";

	//Loop que corre a lista procurando pelo usuario pedido
	for (i = 0; i < cliente_ativo->num_active_users + 1; ++i)
	{
		if (cliente_ativo->list[i].id == ID_int)
			posicao_lista = i;
	}

	// Caso nao encontre, retorna
	if (posicao_lista == -1)
	{
		cout << "Cliente nao existe"<< endl;
		return "";
	}

	num_amostras_int = stoi(num_amostras);

	//Verifica se requisitou somente 1 ou um numero diferente de amostras
	//Se for 1 busca informacao direto com o gateway
	if (num_amostras_int == 1)
	{
		//Acesso a memoria compartilhada
		m.lock();
		if (cliente_ativo->list[posicao_lista].id != -1)
		{
			//Construcao da mensagem
			msg = msg +"1;" + id + ";POS;" + to_string(cliente_ativo->list[posicao_lista].timestamp) + ';' + to_string(cliente_ativo->list[posicao_lista].latitude) + ';' + to_string(cliente_ativo->list[posicao_lista].longitude) + ';' + to_string(cliente_ativo->list[posicao_lista].speed) + ';' + "1\n";
			m.unlock();
			return msg;
		}
		else
		{
			cout << "O cliente nao esta ativo" << endl;
			m.unlock();
			return "";
		}
	}

	//Se o numero de amostras for diferete de 1 deve-se buscar a informacao
	//no historiador
	historical_data_request_t request_h;
	request_h.id = ID_int;
	request_h.num_samples = num_amostras_int;

	historical_data_reply_t reply_h;
	size_t size;
	unsigned int p;

	// Abre a message queue para requisicao de dados
	message_queue mq_send(open_only, "servapp_historiador");

	// Abre a message queue para a resposta
	message_queue mq_recv(open_only, "historiador_servapp");

	// Envia requisicao e recebe resposta
	mq_send.send(&request_h, sizeof(request_h), 0);
	mq_recv.receive(&reply_h, sizeof(reply_h), size, p);

	string numero_amostras_disponiveis = to_string(reply_h.num_samples_available);

	//Se nao existe dados historicos para esse cliente
	if (reply_h.num_samples_available == 0)
	{
		msg = msg + "0\n";
		return msg;
	}

	msg = msg + numero_amostras_disponiveis + ';' + id;
	for (int i = 0; i < reply_h.num_samples_available; i++)
	{
		//Construcao da mensagem
		msg = msg + ";POS;"	+ to_string(reply_h.data[i].timestamp) + ';' + to_string(reply_h.data[i].latitude) + ';' + to_string(reply_h.data[i].longitude) + ';' + to_string(reply_h.data[i].speed) + ';' + "1\n";
	}
	return msg;
}

//Classes necessarias para a comunicacao TCP/IP
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
		//recebe e envia mensagens
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			[this, self](boost::system::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				cout << "Mensagem recebida! IP:Porta: " << socket_.remote_endpoint();
				data_[length] = '\0';
				string saida_funcao;
				int idx = 0;

				//Processa o que foi pedido pela interface grafica
				string comando;
				for (int i = 0; data_[i] != ';' && data_[i] != '\n'; i++)
				{
					comando = comando + data_[i];
				}

				// Se solicitou usuarios ativos, chama respectiva funcao
				if (comando == "REQ_ATIVOS")
				{
					saida_funcao = req_ativos();
				}

				// Se solicitou historica, chama respectiva funcao
				else if (comando == "REQ_HIST")
				{
					// Procura na mensagem recebida quantas amostras
					// de historico foram solicitadas.
					string id;
					string num_amostras;
					int i;
					for (i = 5; data_[i] != ';'; i++)
						id = id + data_[i];
					for (i = i + 1; i < int(length - 1); i++)
					{
						num_amostras = num_amostras + data_[i];
					}

					// Chama funcao para tratar a requisicao
					saida_funcao = req_hist(id, num_amostras);
				}
				// Transfere os valores da string para um vetor de char por causa da
				// async_write_some.
				for (idx = 0; idx < int(saida_funcao.length()); ++idx)
				{
					datasend_[idx] = saida_funcao[idx];
				}
				datasend_[idx] = '\0';
				cout << "Tentando enviar a  mensagem: " << saida_funcao;
				cout << endl;

				//Envia a mensagem e espera por mais mensagens em loop da interface grafica
				socket_.async_write_some(boost::asio::buffer(datasend_, max_length), [this, self](boost::system::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						cout << "Sucesso!" << endl;
						do_read();
					}
				});
				loop();
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
	char datasend_[max_length];
};

// A classe Server fara o papel de servidor, o qual aceita novas conexoes
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
	//A primeira acao do servidor de aplicacao eh abrir a memoria compartilhada
	//com o gateway
	shared_memory_object shm(open_only, "UsuariosAtivos", read_write);
	shm.truncate(1000);
	mapped_region UsuariosAtivos(shm, read_write);
	cliente_ativo = static_cast<active_users_t*>(UsuariosAtivos.get_address());

	// Inicia a comunicacao TCP/IP
	try
	{
		boost::asio::io_service io_service;
		server s(io_service, std::atoi("9001"));
		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Conexao interrompida " << e.what() << "\n";
	}

	return 0;
}
