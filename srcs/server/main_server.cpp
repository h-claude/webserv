/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main_server.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hclaude <hclaude@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/15 14:32:13 by hclaude           #+#    #+#             */
/*   Updated: 2025/06/17 12:03:33 by hclaude          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "webserv.hpp"


int	isCgiRequired(Server *server, Client *client)
{
	int	i;
	std::string	url;

	url = client->getUrl();
	std::cout
	<< "\033[1;34m[Request] \033[0m"              // Bold blue label
	<< "\033[1;32mMethod: \033[0m"                // Bold green label
	<< "\033[1;33m" << client->getMethod() << "\033[0m" // Bold yellow method (e.g., GET, POST)
	<< "  "
	<< "\033[1;36mURL: \033[0m"                   // Bold cyan label
	<< "\033[1;37m" << url << "\033[0m"           // Bold white URL
	<< std::endl;


	i = getRouteIndex(url, server);

	std::string ext = getExtensionFile(client->getUrlPath());

	if (i == -1)
		return (1);

	else if (!(server->routes[i]->cgi_enabled) || server->routes[i]->cgi->extensions.find(getExtensionFile(client->getUrlPath())) == server->routes[i]->cgi->extensions.end())
		return (1);
	return (0);
}

bool	new_client(int i, std::vector<struct pollfd>& poll_fds, std::map<int, int>& client_data, std::map<int, ClientState*>& client_state)
{
	int			client_fd;
	sockaddr_in	client_address;
	socklen_t	client_address_len;
	pollfd		client_tmp;

	client_address_len = sizeof(client_address);
	client_fd = accept(poll_fds[i].fd, (sockaddr*)&client_address, &client_address_len);
	if (client_fd < 0)
		return (false);

	setNonBlocking(client_fd);
	client_data[client_fd] = i;

	if (client_state.find(client_fd) == client_state.end())
	{
		std::cout << GREEN << "NEW CLIENT " << client_fd << END << std::endl;
		ClientState*	newClient = new ClientState();
		client_state[client_fd] = newClient;
		int fd[2] = {-2, -2};
		client_state[client_fd]->setPipeIn(fd);
		client_state[client_fd]->setPipeOut(fd);
		client_state[client_fd]->setFdTmp(0);
	}

	client_tmp.fd = client_fd;
	client_tmp.events = POLLIN;
	client_tmp.revents = 0;
	poll_fds.push_back(client_tmp);
	return (true);
}

bool get_request_body(int fd, const std::string& last, std::map<int, ClientState*>& client_state) {

	if (!client_state[fd]->is_file_initialized()) {
		if (!client_state[fd]->init_upload(fd)) {
			std::cerr << "Failed to open upload file" << std::endl;
			return false;
		}
	}
	if (!last.empty())
		client_state[fd]->append_data(last.c_str(), last.size());

	if (client_state[fd]->getContentLength() == 0) {
		client_state[fd]->finalize_upload();
		return true;
	}

	char buffer[1024 * 1024];
	ssize_t bytes_received = recv(fd, buffer, sizeof(buffer), 0);

	if (bytes_received > 0)
		client_state[fd]->append_data(buffer, bytes_received);
	else if (bytes_received == 0)
	{
		client_state[fd]->finalize_upload();
		return true;
	}

	if (client_state[fd]->is_upload_complete())
		client_state[fd]->finalize_upload();

	return true;
}

bool is_valid_number(const std::string& str) {
	if (str.empty()) return false;
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
		if (!isdigit(*it)) return false;
	return true;
}

bool getContentLength(const std::string& header, unsigned long long &content_length) {
	std::string key = "Content-Length:";
	size_t pos = header.find(key);
	if (pos == std::string::npos)
		return false;
	size_t end = header.find("\r\n", pos);
	std::string len_str = trim(header.substr(pos + key.length(), end - (pos + key.length())));
	if (len_str.empty())
		return false;
	if (!is_valid_number(len_str))
		return false;
	unsigned long long result = 0;
	for (size_t i = 0; i < len_str.size(); ++i) {
		if (result > ULLONG_MAX / 10) {
			return false;
		}
		result = result * 10 + (len_str[i] - '0');
	}
	content_length = result;
	return true;
}

std::string extractHostFromHeader(const std::string& header) {
	size_t pos = header.find("Host:");
	if (pos == std::string::npos)
		return "";
	size_t start = header.find_first_not_of(" ", pos + 5);
	size_t end = header.find("\r\n", start);
	return header.substr(start, end - start);
}


int	get_header(int fd, std::map<int, ClientState*> &client_state, Server* server, Client* client)
{
	ClientState* state = client_state[fd];
	if (!state) {
		std::cerr << RED << "Invalid client state" << END << std::endl;
		return -1;
	}

	char	buffer[8 * 1024];
	ssize_t	bytes_received = 0;

	if (client_state[fd]->getClientState() == WAITING_HEADERS)
	{
		bytes_received = recv(fd, buffer, sizeof(buffer), 0);
		if (bytes_received == 0)
			return (0);
		else if (bytes_received < 0)
			return 1;

		client_state[fd]->appendToBufferHeader(buffer, bytes_received);
		const std::string &raw = client_state[fd]->getBufferHeader();
		size_t header_end = client_state[fd]->getBufferHeader().find("\r\n\r\n");
		if (header_end == std::string::npos)
			return 1;

		std::string header = raw.substr(0, header_end + 4);
		std::string body = raw.substr(header_end + 4);
		client_state[fd]->setBufferHeader(header);

		unsigned long long content_length = 0;
		client_state[fd]->setHasContentLength(getContentLength(header, content_length));
		client_state[fd]->setContentLength(client_state[fd]->getHasContentLength() ? content_length : 0);
		std::string host = extractHostFromHeader(header);
		const Server *vhost = ServersDatas::getInstance()->findServerConfig(host, server->port);
		(void)client;
		(void)server;
		if (vhost && state->getContentLength() > vhost->client_max_body_size)
		{
			my_usleep(20000);
			std::string error_response = client->pageError(413, (Server *)vhost);
			if (send(fd, error_response.c_str(), error_response.size(), 0) <= 0)
				return (-1);
			return (-2);
		}
		client_state[fd]->setClientState(WAITING_BODY);
		get_request_body(fd, body, client_state);
		memset(buffer, 0, sizeof(buffer));

		return 1;
	}

	if (client_state[fd]->getClientState() == WAITING_BODY)
		get_request_body(fd, "", client_state);

	if (client_state[fd]->getClientState() != WAITING_HEADERS)
	{
		std::cout << std::endl;
		std::cout << "\033[1m[Content-Length Check] \033[0m"
				  << "Received: \033[36m"
				  << client_state[fd]->getCurrentContentLength()
				  << "\033[0m"
				  << " / Expected: \033[32m"
				  << client_state[fd]->getContentLength()
				  << "\033[0m"
				  << " -> Status: ";
	}
	return (1);
}


std::string redirect(std::string& link)
{
	std::string response;
	std::string body = "<html><head><title>301 Moved Permanently</title></head><body><h1>Moved Permanently</h1><p>Ressource moved to <a href=\"" + link + "\">this address</a>.</p></body></html>";

	std::stringstream ss;
	ss << body.length();

	response.append("HTTP/1.1 301 Moved Permanently\r\n");
	response.append("Location: " + link + "\r\n");
	response.append("Content-Type: text/html\r\n");
	response.append("Content-Length: " + ss.str() + "\r\n");
	response.append("\r\n");
	response.append(body);

	std::cout << BOLD << response << END << std::endl;

	return (response);
}

std::string	execGET(Client* client, int index_server, ServersDatas* serverdatas, int fd, std::map<int, ClientState*> client_state, std::vector<struct pollfd>& poll_fds)
{
	std::string	response;
	int			i;

	i = getRouteIndex(client->getUrl(), serverdatas->server[index_server]);
	if (i == -1)
		return (client->pageError(405, serverdatas->server[index_server]));

	if (!serverdatas->server[index_server]->routes[i]->redirect.empty())
	{
		response = redirect(serverdatas->server[index_server]->routes[i]->redirect);
		return (response);
	}

	if (!isMethodhere((std::string&)client->getMethod(), serverdatas->server[index_server]->routes[i]))
		return (client->pageError(405, serverdatas->server[index_server]));

	if (!client->checkPageExists())
	{
		if (!isCgiRequired(serverdatas->server[index_server], client))
			response = client->cgi(serverdatas->server[index_server], *client_state[fd], poll_fds, i);
		else
			response = client->convertRequestForSend(serverdatas->server[index_server]->routes[i], serverdatas->server[index_server]);
	}
	else
		response = client->pageError(404, serverdatas->server[index_server]);

	if (client_state[fd]->getCurrentContentLengthCGI() == client_state[fd]->getContentLength())
		client_state[fd]->setClientState(RESPONDING);
	else
		std::cout << RED << "Current content length is currently: " << client_state[fd]->getCurrentContentLengthCGI() << " / " <<  client_state[fd]->getContentLength() << END << std::endl;
	return (response);
}

std::string	execPOST(Client* client, int index_server, ServersDatas* serverdatas, int fd, std::map<int, ClientState*> client_state, std::vector<struct pollfd>& poll_fds)
{
	std::string	response;
	int			i;

	i = getRouteIndex(client->getUrl(), serverdatas->server[index_server]);
	if (i == -1)
		return (client->pageError(405, serverdatas->server[index_server]));

	if (!serverdatas->server[index_server]->routes[i]->redirect.empty())
	{
		response = redirect(serverdatas->server[index_server]->routes[i]->redirect);
		return (response);
	}

	if (!isMethodhere((std::string&)client->getMethod(), serverdatas->server[index_server]->routes[i]))
		return (client->pageError(405, serverdatas->server[index_server]));

	if (!client->checkPageExists())
	{
		if (!isCgiRequired(serverdatas->server[index_server], client))
			response = client->cgi(serverdatas->server[index_server], *client_state[fd], poll_fds, i);
		else
			response = client->convertRequestForSend(serverdatas->server[index_server]->routes[i], serverdatas->server[index_server]);
	}
	else
		response = client->pageError(404, serverdatas->server[index_server]);

	if (client_state[fd]->getCurrentContentLengthCGI() == client_state[fd]->getContentLength())
		client_state[fd]->setClientState(RESPONDING);
	else
		std::cout << RED << "Current content length is currently: " << client_state[fd]->getCurrentContentLengthCGI() << " / " <<  client_state[fd]->getContentLength() << END << std::endl;
	return (response);
}

std::string urlDecode(const std::string& str) {
	std::string result;
	char ch;
	std::string::size_type i, ii;
	for (i = 0; i < str.length(); i++) {
		if (str[i] == '%') {
			if (i + 2 < str.length()) {
				std::istringstream iss(str.substr(i + 1, 2));
				iss >> std::hex >> ii;
				ch = static_cast<char>(ii);
				result += ch;
				i += 2;
			}
		} else if (str[i] == '+') {
			result += ' ';
		} else {
			result += str[i];
		}
	}
	return result;
}

std::string	execDELETE(Client* client, int index_server, ServersDatas* serverdatas)
{
	std::string	response;
	int			i;
	std::string filepath = client->getUrlPath();
	filepath = urlDecode(filepath);

	i = -1;

	i = getRouteIndex(client->getUrl(), serverdatas->server[index_server]);
	if (i == -1)
		return (client->pageError(405, serverdatas->server[index_server]));

	if (!serverdatas->server[index_server]->routes[i]->redirect.empty())
	{
		response = redirect(serverdatas->server[index_server]->routes[i]->redirect);
		return (response);
	}
	if (!isMethodhere((std::string&)client->getMethod(), serverdatas->server[index_server]->routes[i]))
		return (client->pageError(405, serverdatas->server[index_server]));

	if (access(filepath.c_str(), F_OK) != 0)
		return client->pageError(404, serverdatas->server[index_server]);

	if (access(filepath.c_str(), W_OK) != 0)
		return (client->pageError(403, serverdatas->server[index_server]));

	if (remove(filepath.c_str()) != 0)
		return client->pageError(500, serverdatas->server[index_server]);

	return (client->pageError(204, serverdatas->server[index_server]));
}

bool	process_request(Client* client, ServersDatas* serversdatas, size_t& i, std::map<int, int>& client_data, std::vector<struct pollfd>& poll_fds, std::map<int, ClientState*> client_state)
{
	std::string	request;
	int			index_server;
	int			bytes_received;
	std::string	response;

	if (!client || !serversdatas || i >= poll_fds.size()) {
		std::cerr << "Invalid parameters" << std::endl;
		return false;
	}

	int fd = poll_fds[i].fd;
	if (client_state.find(fd) == client_state.end()) {
		std::cerr << "No client state for FD " << fd << std::endl;
		return false;
	}
	index_server = client_data[fd];

	bytes_received = get_header(fd, client_state, serversdatas->server[index_server], client);
	if (bytes_received == -2)
	{
		if (client->getKeepAlive())
		{
			client_state[fd]->setCurrentContentLengthCGI(0);
			client_state[fd]->reset();
		}
		else
		{
			std::cout << RED << "Deleted the client!" << END << std::endl;
			close(fd);
			client_state.erase(fd);
			client_data.erase(fd);
			poll_fds.erase(poll_fds.begin() + i);
		}
	}
	if (bytes_received <= 0 || (poll_fds[i].revents & (POLLERR | POLLHUP)))
	{
		close(fd);
		client_state.erase(fd);
		client_data.erase(fd);
		poll_fds.erase(poll_fds.begin() + i);
		return (true);
	}

	if (index_server < 0 || index_server >= (int)serversdatas->server.size())
	{
		std::cerr << "Error: invalid server index" << std::endl;
		std::string error_response = client->pageError(500, serversdatas->server[0]);
		if (send(fd, error_response.c_str(), error_response.size(), 0) <= 0)
		{
			close(fd);
			client_state.erase(fd);
			client_data.erase(fd);
			poll_fds.erase(poll_fds.begin() + i);
			return (false);
		}
		return (false);
	}
	if (client_state[fd]->getClientState() == PROCESSING)
	{
		if (!client_state[fd]->getBufferHeader().empty())
		{
			if (client->parseClientRequest(client_state[fd]->getBufferHeader(), client) == -1)
			{
				std::cout << client_state[fd]->getBufferHeader() << std::endl;
				response = client->pageError(405, serversdatas->server[0]);
				client_state[fd]->setClientState(RESPONDING);
			}
			client_state[fd]->setBufferHeader("");
			client->resolveVirtualServer(client, serversdatas->server[index_server]);
			const std::string method = client->getMethod();

			if (method == "GET")
			{
				response = execGET(client, index_server, serversdatas, fd, client_state, poll_fds);
				client_state[fd]->setClientState(RESPONDING);
			}
			else if (method == "POST")
				response = execPOST(client, index_server, serversdatas, fd, client_state, poll_fds);
			else if (method == "DELETE")
			{
				std::cout << std::endl;
				std::cout << std::endl;
				std::cout << "\033[31;1m"; // Cyan gras pour les bordures
				std::cout << "==============================\n";

				std::cout << "\033[31;1m"; // Rouge gras pour le titre
				std::cout << " 🗑️  Suppression de fichier\n";

				std::cout << "\033[31;1m"; // Retour au cyan pour la suite
				std::cout << "------------------------------\n";
				std::cout << " ➤ Fichier ciblé : " << client->getUrl() << "\n";
				std::cout << "==============================\n";
				std::cout << " Veuillez patienter...\n\n";

				std::cout << "\033[0m"; // Reset
				response = execDELETE(client, index_server, serversdatas);
				std::cout << "\033[32;1m"; // Vert en gras
				std::cout << "✅ Succès : Le fichier \"" << client->getUrl() << "\" a été supprimé avec succès !\n";
				std::cout << "\033[0m"; // Reset
				std::cout << std::endl;
				client_state[fd]->setClientState(RESPONDING);
			}
			else
				response = client->pageError(405, serversdatas->server[index_server]);
		}
	}

	if (client_state[fd]->getClientState() == RESPONDING)
	{
		std::string connLine = "Connection: "
			+ std::string(client->getKeepAlive() ? "keep-alive" : "close")
			+ "\r\n";
		size_t hdrEnd = response.find("\r\n\r\n");
		if (hdrEnd != std::string::npos) {
			response.insert(hdrEnd + 2, connLine);
		} else {
			response = connLine + "\r\n" + response;
		}
		if (send(fd, response.c_str(), response.size(), 0) <= 0)
		{
			std::cerr << "Error sending response." << std::endl;
			close(fd);
			client_state.erase(fd);
			client_data.erase(fd);
			poll_fds.erase(poll_fds.begin() + i);
		}
		if (client->getKeepAlive())
		{
			client_state[fd]->setCurrentContentLengthCGI(0);
			client_state[fd]->reset();
		}
		else
		{
			std::cout << RED << "Deleted the client!" << END << std::endl;
			close(fd);
			client_state.erase(fd);
			client_data.erase(fd);
			poll_fds.erase(poll_fds.begin() + i);
		}
	}

	return (true);
}

void	close_poll_fds(std::vector<struct pollfd>& poll_fds)
{
	for (size_t i = 0; i < poll_fds.size(); i++)
	{
		if (poll_fds[i].fd != -1) {
			close(poll_fds[i].fd);
			poll_fds[i].fd = -1;
		}

	}
}

bool writing_CGI(std::map<int, ClientState*>&		client_state, std::vector<struct pollfd>&		poll_fds, size_t& i)
{
	std::cout << "path: " << client_state[poll_fds[i].fd]->getFdTmp() << std::endl;

	if (client_state[poll_fds[i].fd]->getFdTmp() < 0)
	{
		std::cerr << RED << "Error opening temporary file for CGI on fd "
			<< poll_fds[i].fd << END << std::endl;
		return (false);
	}

	char buffer[4096]; // Buffer size can be adjusted as needed
	ssize_t bytes_read = 0;

	bytes_read = read(client_state[poll_fds[i].fd]->getFdTmp(), buffer, sizeof(buffer));
	if (bytes_read < 0)
	{
		std::cerr << BOLD_RED << "Error: bytes_read < 0!" << END << std::endl;
		return (true);
	}
	else if (bytes_read == 0)
	{
		std::cerr << BOLD_RED << "Error: bytes_read == 0!" << END << std::endl;
	}

	 ssize_t written = write(client_state[poll_fds[i].fd]->getPipeIn(1), buffer, bytes_read);
	 if (written < 0)
	 {
		std::cerr << BOLD_RED << "Error: written < 0!" << END << std::endl;
	 }
	 else if (written == 0)
	 {
		std::cerr << BOLD_RED << "Error: written == 0!" << END << std::endl;
	 }

	client_state[poll_fds[i].fd]->setCurrentContentLengthCGI(client_state[poll_fds[i].fd]->getCurrentContentLengthCGI() + bytes_read);
	std::cout
	<< "\033[1;34m[CGI Progress] \033[0m"           // Bold blue
	<< "\033[1;32mWritten: \033[0m"                  // Bold green
	<< "\033[1;33m" << client_state[poll_fds[i].fd]->getCurrentContentLengthCGI() << "\033[0m"  // Bold yellow
	<< "\033[1;32m / \033[0m"                        // Bold green
	<< "\033[1;36m" << client_state[poll_fds[i].fd]->getContentLength() << "\033[0m"            // Bold cyan
	<< "\033[1;37m bytes" << poll_fds[i].fd << "\033[0m"                      // Bold white
	<< std::endl;

	if (client_state[poll_fds[i].fd]->getCurrentContentLengthCGI() >= client_state[poll_fds[i].fd]->getContentLength())
	{
		close(client_state[poll_fds[i].fd]->getFdTmp());
		client_state[poll_fds[i].fd]->setClientState(CGI_READING);
		return (false);
	}
	return (true);
}

bool reading_CGI(std::map<int, ClientState*>&		client_state, std::vector<struct pollfd>&		poll_fds, size_t& i, Client* client)
{
	std::string total_output;
	std::string header;
	std::string ret_str;
	close(client_state[poll_fds[i].fd]->getPipeIn(1));
	ssize_t byte_read;
	char buf[16*1024];

	my_usleep(4000);
	byte_read = read(client_state[poll_fds[i].fd]->getPipeOut(0), buf, sizeof(buf));
	if (byte_read < 0)
		return false;
	else if (byte_read == 0)
	{
		std::cout << BOLD_PURPLE << "No more data to read from CGI output pipe on fd "
			<< poll_fds[i].fd << END << std::endl;
		close(client_state[poll_fds[i].fd]->getPipeOut(0));
		return true;
	}
	total_output.append(buf, byte_read);
	close(client_state[poll_fds[i].fd]->getPipeOut(0));

	hideHeaderCGI(total_output);

	//init my header
	struct t_header param;
	client->initHeader(param, total_output.size());
	client->setHeader(param, header);
	ret_str = header + total_output;

	if (send(poll_fds[i].fd, ret_str.c_str(), ret_str.size(), 0) == 0)
	{
		close(poll_fds[i].fd);
		client_state.erase(poll_fds[i].fd);
		poll_fds.erase(poll_fds.begin() + i);
		return (true);
	}

	if (client->getKeepAlive())
	{
		client_state[poll_fds[i].fd]->setCurrentContentLengthCGI(0);
		client_state[poll_fds[i].fd]->reset();
	}
	return true;
}

int	launch_server(ServersDatas* serversdatas, Client* client)
{
	std::vector<struct pollfd>		poll_fds; //liste de descripteurs de fichiers à surveiller
	std::map<int, ClientState*>		client_state; //file descriptor client, info plus complète
	std::map<int, int>				client_data;
	int								ret = 0;

	poll_fds = initPollFds(serversdatas);
	if (poll_fds.empty())
	{
		std::cerr << "Error: No server socket initialized";
		return (1);
	}

	while (server_is_running)
	{
		ret = poll(poll_fds.data(), poll_fds.size(), 500);
		if (ret == -1)
		{
			for (std::map<int, ClientState*>::iterator it = client_state.begin(); it != client_state.end(); ++it)
			{
				if (it->second->getPipeIn(0) != -1)
					close(it->second->getPipeIn(0));
				if (it->second->getPipeIn(1) != -1)
					close(it->second->getPipeIn(1));
				if (it->second->getPipeOut(0) != -1)
					close(it->second->getPipeOut(0));
				if (it->second->getPipeOut(1) != -1)
					close(it->second->getPipeOut(1));
				if (it->second->getFdTmp() != -1)
					close(it->second->getFdTmp());
				delete it->second;
			}
			client_state.clear();
			close_poll_fds(poll_fds);
			return 1;
		}
		for (size_t i = poll_fds.size(); i-- > 0 ;)
		{
			if (client_state.find(poll_fds[i].fd) == client_state.end() && is_server(poll_fds[i].fd, serversdatas))
			{
				if (!new_client(i, poll_fds, client_data, client_state))
					continue;
			}
			else if (client_state.find(poll_fds[i].fd) != client_state.end() \
			&& (client_state[poll_fds[i].fd]->getClientState() == WAITING_HEADERS || client_state[poll_fds[i].fd]->getClientState() == WAITING_BODY) \
			&& poll_fds[i].revents & POLLIN)
			{
				if (!process_request(client, serversdatas, i, client_data, poll_fds, client_state))
					continue ;
			}
			else if (client_state.find(poll_fds[i].fd) != client_state.end() && client_state[poll_fds[i].fd]->getClientState() == CGI_WRITING)
			{
				if (writing_CGI(client_state, poll_fds, i))
					continue ;
			}
			else if (client_state.find(poll_fds[i].fd) != client_state.end() && client_state[poll_fds[i].fd]->getClientState() == CGI_READING)
			{
				if (reading_CGI(client_state, poll_fds, i, client))
					continue ;
			}
			else if (poll_fds[i].revents & POLLHUP || poll_fds[i].revents & POLLERR || poll_fds[i].revents & POLLNVAL)
			{
				poll_fds.erase(poll_fds.begin() + i);
				continue ;
			}

		}
	}

	for (std::map<int, ClientState*>::iterator it = client_state.begin(); it != client_state.end(); ++it)
	{
		std::cout << "DELETEE" << std::endl;
		// sleep(5);
		if (it->second->getPipeIn(0) != -1)
			close(it->second->getPipeIn(0));
		if (it->second->getPipeIn(1) != -1)
			close(it->second->getPipeIn(1));
		if (it->second->getPipeOut(0) != -1)
			close(it->second->getPipeOut(0));
		if (it->second->getPipeOut(1) != -1)
			close(it->second->getPipeOut(1));
		if (it->second->getFdTmp() != -1)
			close(it->second->getFdTmp());
		delete it->second;
	}
	client_state.clear();
	close_poll_fds(poll_fds);
	return (0);
}

