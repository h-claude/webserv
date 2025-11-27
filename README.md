# Webserv 🌐

A high-performance HTTP/1.1 web server written in C++98, compliant with the 42 school project requirements.

## Features

- **HTTP/1.1 compliant** - Supports GET, POST, and DELETE methods
- **Multiple server blocks** - Run multiple virtual servers on different ports
- **CGI support** - Execute PHP and Python scripts
- **File uploads** - Handle file uploads with configurable size limits
- **Custom error pages** - Define custom error pages for different HTTP error codes
- **URL redirection** - Configure HTTP redirects
- **Directory listing** - Optional auto-index for directories
- **Virtual hosting** - Route requests based on the `Host` header
- **Keep-alive connections** - Persistent connections support
- **Non-blocking I/O** - Uses `poll()` for efficient connection handling

## Requirements

- C++ compiler with C++98 support (g++ or clang++)
- Make
- Linux or macOS operating system
- For CGI:
  - Python 3 (`/usr/bin/python3`) for `.py` scripts
  - PHP-CGI (`/usr/bin/php-cgi`) for `.php` scripts

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/h-claude/webserv.git
   cd webserv
   ```

2. Compile the project:
   ```bash
   make
   ```

3. Run the server with a configuration file:
   ```bash
   ./webserv conf/one_server.conf
   ```

## Usage

```bash
./webserv <configuration_file.conf>
```

The server will start listening on the ports specified in the configuration file. Press `Ctrl+C` to gracefully shut down the server.

### Example

```bash
# Start server on port 8080
./webserv conf/one_server.conf

# Access the server
curl http://localhost:8080/
```

## Configuration

The server uses a JSON-like configuration format. Configuration files must have a `.conf` extension.

### Basic Configuration Structure

```conf
server:
{
    host: "0.0.0.0",
    port: 8080,
    server_names: ["example.com", "www.example.com"],
    default: true,
    client_max_body_size: 10000000,
    error_pages:
    {
        404: "/errors/404.html",
        403: "/errors/403.html",
        500: "/errors/500.html"
    },
    routes:
    [
        {
            path: "/",
            methods: ["GET", "POST", "DELETE"],
            root: "./srcs/html/www",
            index: "index.html",
            autoindex: false,
            upload_dir: "/tmp/uploads",
            cgi:
            {
                enabled: true,
                extensions:
                {
                    ".py": "/usr/bin/python3",
                    ".php": "/usr/bin/php-cgi"
                }
            }
        }
    ]
}
```

### Configuration Options

#### Server Block

| Option | Description | Example |
|--------|-------------|---------|
| `host` | IP address to bind to | `"0.0.0.0"` |
| `port` | Port number to listen on | `8080` |
| `server_names` | List of server names for virtual hosting | `["example.com"]` |
| `default` | Set as default server for this port | `true` |
| `client_max_body_size` | Maximum request body size in bytes | `10000000` |
| `error_pages` | Custom error page paths | See example above |

#### Route Block

| Option | Description | Example |
|--------|-------------|---------|
| `path` | URL path for this route | `"/"` |
| `methods` | Allowed HTTP methods | `["GET", "POST"]` |
| `root` | Root directory for serving files | `"./srcs/html/www"` |
| `index` | Default index file | `"index.html"` |
| `autoindex` | Enable directory listing | `false` |
| `upload_dir` | Directory for file uploads | `"/tmp/uploads"` |
| `redirect` | URL to redirect to | `"https://example.com"` |
| `cgi` | CGI configuration block | See CGI section |

### CGI Configuration

```conf
cgi:
{
    enabled: true,
    extensions:
    {
        ".py": "/usr/bin/python3",
        ".php": "/usr/bin/php-cgi"
    }
}
```

CGI scripts receive standard CGI environment variables and can process both GET and POST requests.

### Multiple Servers

You can define multiple server blocks in a single configuration file:

```conf
server:
{
    port: 8080,
    # ... configuration
}

server:
{
    port: 4040,
    # ... configuration
}
```

## Configuration Examples

Example configuration files are provided in the `conf/` directory:

- `one_server.conf` - Single server setup
- `multiple_server.conf` - Multiple servers on different ports
- `exemple.conf` - Basic example configuration

## Project Structure

```
webserv/
├── Makefile
├── conf/                    # Configuration files
├── includes/                # Header files
│   ├── Client.hpp
│   ├── Server.hpp
│   └── webserv.hpp
└── srcs/
    ├── main.cpp             # Entry point
    ├── Client.cpp           # Client handling
    ├── ClientState.cpp      # Client state management
    ├── html/                # Web content
    │   └── www/             # Default web root
    ├── parsing_conf/        # Configuration parser
    └── server/              # Server implementation
```

## Make Commands

| Command | Description |
|---------|-------------|
| `make` | Compile the project |
| `make clean` | Remove object files |
| `make fclean` | Remove object files and executable |
| `make re` | Recompile the project |

## Authors

- hclaude
- ldoppler

## License

This project is part of the 42 school curriculum.
