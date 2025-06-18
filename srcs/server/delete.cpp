/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   delete.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hclaude <hclaude@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 21:00:38 by hclaude           #+#    #+#             */
/*   Updated: 2025/06/07 21:23:36 by hclaude          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "webserv.hpp"

#include "Server.hpp"

CGI::~CGI()
{
	extensions.clear();
}

Route::~Route()
{
	if (cgi)
	{
		delete cgi;
		cgi = 0;
	}
}

ErrorPages::~ErrorPages()
{
	pages.clear();
}

ServerSocket::~ServerSocket()
{
	if (sockfd != -1)
	{
		close(sockfd);
		sockfd = -1;
	}
}

Server::~Server()
{
	if (error_pages)
	{
		delete error_pages;
		error_pages = 0;
	}
	for (size_t i = 0; i < routes.size(); i++)
	{
		delete routes[i];
	}
	if (serverSocket != 0)
	{
		delete serverSocket;
		serverSocket = 0;
	}
}

ServersDatas::~ServersDatas()
{
	for (size_t i = 0; i < server.size(); i++)
	{
		delete server[i];
	}
	server.clear();
}
