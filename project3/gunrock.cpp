#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

vector<HttpService *> services;

deque<MySocket*> request_buffer;

pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;

HttpService *find_service(HTTPRequest *request) {

  for (unsigned int idx = 0; idx < services.size(); idx++) {

    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}

void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {

  stringstream payload;

  if (service == NULL) {

    response->setStatus(404);

  } else if (request->isHead()) {

    service->head(request, response);

  } else if (request->isGet()) {

    service->get(request, response);

  } else {

    response->setStatus(501);
  }
}

void handle_request(MySocket *client) {

  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();

  stringstream payload;

  bool readResult = false;

  try {

    payload << "client: " << (void *) client;

    sync_print("read_request_enter", payload.str());

    readResult = request->readRequest();

    sync_print("read_request_return", payload.str());

  } catch (...) {

  }

  if (!readResult) {

    delete response;
    delete request;

    sync_print("read_request_error", payload.str());

    return;
  }

  HttpService *service = find_service(request);

  invoke_service_method(service, request, response);

  payload.str("");
  payload.clear();

  payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;

  sync_print("write_response", payload.str());

  cout << payload.str() << endl;

  client->write(response->response());

  delete response;
  delete request;

  payload.str("");
  payload.clear();

  payload << " client: " << (void *) client;

  sync_print("close_connection", payload.str());

  client->close();

  delete client;
}

void* worker_thread(void* arg) {

  while (true) {

    dthread_mutex_lock(&buffer_lock);

    while (request_buffer.empty()) {

      dthread_cond_wait(&buffer_not_empty, &buffer_lock);
    }

    MySocket* client = request_buffer.front();

    request_buffer.pop_front();

    dthread_cond_signal(&buffer_not_full);

    dthread_mutex_unlock(&buffer_lock);

    handle_request(client);
  }

  return NULL;
}

int main(int argc, char *argv[]) {

  signal(SIGPIPE, SIG_IGN);

  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {

    switch (option) {

      case 'd':
        BASEDIR = string(optarg);
        break;

      case 'p':
        PORT = atoi(optarg);
        break;

      case 't':
        THREAD_POOL_SIZE = atoi(optarg);
        break;

      case 'b':
        BUFFER_SIZE = atoi(optarg);
        break;

      case 's':
        SCHEDALG = string(optarg);
        break;

      case 'l':
        LOGFILE = string(optarg);
        break;

      default:
        cerr << "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
        exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");

  MyServerSocket *server = new MyServerSocket(PORT);

  MySocket *client;

  services.push_back(new FileService(BASEDIR));

  vector<pthread_t> workers(THREAD_POOL_SIZE);

  for (int i = 0; i < THREAD_POOL_SIZE; i++) {

    dthread_create(&workers[i], NULL, worker_thread, NULL);

    dthread_detach(workers[i]);
  }

  while (true) {

    sync_print("waiting_to_accept", "");

    client = server->accept();

    sync_print("client_accepted", "");

    dthread_mutex_lock(&buffer_lock);

    while ((int)request_buffer.size() >= BUFFER_SIZE) {

      dthread_cond_wait(&buffer_not_full, &buffer_lock);
    }

    request_buffer.push_back(client);

    dthread_cond_signal(&buffer_not_empty);

    dthread_mutex_unlock(&buffer_lock);
  }
}