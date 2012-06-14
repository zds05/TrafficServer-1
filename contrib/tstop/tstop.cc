/** @file

    Main file for the tstop application.

    @section license License

    Licensed to the Apache Software Foundation (ASF) under one
    or more contributor license agreements.  See the NOTICE file
    distributed with this work for additional information
    regarding copyright ownership.  The ASF licenses this file
    to you under the Apache License, Version 2.0 (the
    "License"); you may not use this file except in compliance
    with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
#include <ncurses.h>
#include <list>
#include <string.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <map>
#include <stdlib.h>
#include "stats.h"
#include <unistd.h>
#include <getopt.h>

using namespace std;
char curl_error[CURL_ERROR_SIZE];
string response;


namespace colorPair {
  const short red = 1;
  const short yellow = 2;
  const short green = 3;
  const short blue = 4;
  const short black = 5;
  const short grey = 6;
  const short cyan = 7;
  const short border = 8;
};


//----------------------------------------------------------------------------
int printGraphLabels(int x, int y, int width, list<const char*> lables) {
  char buffer[256];
  for (list<const char*>::const_iterator it = lables.begin();
       it != lables.end(); ++it) {

    char *pos = buffer;
    pos += snprintf(buffer, 256, "%7s [", *it);
    memset(pos, ' ', width);
    pos += width - 1;
    
    strncat(pos, "]", buffer - pos);
    
    mvprintw(y++, x, buffer);
  }
  
  //  mvprintw(x, y, 
}


//----------------------------------------------------------------------------
void prettyPrint(const int x, const int y, const double number, const int type) {
  char buffer[32];
  char exp = ' ';
  double my_number = number;
  short color;
  if (number > 1000000000000LL) {
    my_number = number / 1000000000000;
    exp = 'T';
    color = colorPair::red;
  } else if (number > 1000000000) {
    my_number = number / 1000000000;
    exp = 'G';
    color = colorPair::red;
  } else if (number > 1000000) {
    my_number = number / 1000000;
    exp = 'M';
    color = colorPair::yellow;
  } else if (number > 1000) {
    my_number = number / 1000;
    exp = 'K';
    color = colorPair::cyan;
  } else if (my_number <= .09) {
    color = colorPair::grey;
  } else {
    color = colorPair::green;
  }

  if (type == 4 || type == 5) {
    if (number > 90) {
      color = colorPair::red;
    } else if (number > 80) {
      color = colorPair::yellow;
    } else if (number > 50) {
      color = colorPair::blue;
    } else if (my_number <= .09) {
      color = colorPair::grey;
    } else {
      color = colorPair::green;
    }
    snprintf(buffer, sizeof(buffer), "%6.1f%%%%", (double)my_number);
  } else
    snprintf(buffer, sizeof(buffer), "%6.1f%c", (double)my_number, exp);
  attron(COLOR_PAIR(color));
  attron(A_BOLD);
  mvprintw(y, x, buffer);
  attroff(COLOR_PAIR(color));
  attroff(A_BOLD);
}

//----------------------------------------------------------------------------
void makeTable(const int x, const int y, const list<string> &items, Stats &stats) {
  int my_y = y;

  for (list<string>::const_iterator it = items.begin(); it != items.end(); ++it) {
    string prettyName;
    double value;
    int type;

    stats.getStat(*it, value, prettyName, type);
    mvprintw(my_y, x, prettyName.c_str());
    prettyPrint(x + 10, my_y++, value, type);
  }
}

//----------------------------------------------------------------------------
size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  response.append((char*)ptr, size * nmemb);
  //cout << "appending: " << size * nmemb << endl;
  //int written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return size * nmemb;
}

//----------------------------------------------------------------------------
void help(const string &host, const string &version) {

  timeout(1000);

  while(1) {
    clear();
    time_t now = time(NULL);
    struct tm *nowtm = localtime(&now);
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", nowtm);

    //clear();
    attron(A_BOLD); mvprintw(0, 0, "Overview:"); attroff(A_BOLD); 
    mvprintw(1, 0, "tstop is a top like program for Apache Traffic Server (ATS).  There is a lot of statistical information gathered by ATS.  This program tries to show some of the more important stats and gives a good overview of what the proxy server is doing.  Hopefully this can be used as a tool to diagnosing the proxy server is there are problems.");

    attron(A_BOLD); mvprintw(7, 0, "Definitions:"); attroff(A_BOLD); 
    mvprintw(8, 0, "Fresh      => Requests that were servered by fresh entries in cache");
    mvprintw(9, 0, "Revalidate => Requests that contacted the origin to verify if still valid");
    mvprintw(10, 0, "Cold       => Requests that were not in cache at all");
    mvprintw(11, 0, "Changed    => Requests that required entries in cache to be updated");
    mvprintw(12, 0, "Changed    => Requests that can't be cached for some reason");
    mvprintw(12, 0, "No Cache   => Requests that the client sent Cache-Control: no-cache header");

    attron(COLOR_PAIR(colorPair::border));
    attron(A_BOLD);
    mvprintw(23, 0, "%s - %.12s - %.12s      (b)ack                            ", timeBuf, version.c_str(), host.c_str());
    attroff(COLOR_PAIR(colorPair::border));
    attroff(A_BOLD);
    refresh();
    int x = getch();
    if (x == 'b')
      break;
  }
}

void usage(char **argv) {
  fprintf(stderr, "Usage: %s [-s seconds] hostname|hostname:port\n", argv[0]);
  exit(1);
}


//----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  int sleep_time = 5000;
  bool absolute = false;
  int opt;
  while ((opt = getopt(argc, argv, "s:")) != -1) {
    switch(opt) {
    case 's':
      sleep_time = atoi(optarg) * 1000;
      break;
    default:
      usage(argv);
    }
  }

  string host = "";
  if (optind >= argc) {
    if (TS_ERR_OKAY != TSInit(NULL, static_cast<TSInitOptionT>(TS_MGMT_OPT_NO_EVENTS | TS_MGMT_OPT_NO_SOCK_TESTS))) {
      fprintf(stderr, "Error: missing hostname on command line or error connecting to the local manager\n");
      usage(argv);
    }
  } else {
    host = argv[optind];
  }
  Stats stats(host);

  if (host == "") {
    char hostname[25];
    hostname[sizeof(hostname) - 1] = '\0';
    gethostname(hostname, sizeof(hostname) - 1);
    host = hostname;
  }

  initscr();
  curs_set(0);
  

  start_color();			/* Start color functionality	*/
  
  init_pair(colorPair::red, COLOR_RED, COLOR_BLACK);
  init_pair(colorPair::yellow, COLOR_YELLOW, COLOR_BLACK);
  init_pair(colorPair::grey, COLOR_BLACK, COLOR_BLACK);
  init_pair(colorPair::green, COLOR_GREEN, COLOR_BLACK);
  init_pair(colorPair::blue, COLOR_BLUE, COLOR_BLACK);
  init_pair(colorPair::cyan, COLOR_CYAN, COLOR_BLACK);
  init_pair(colorPair::border, COLOR_WHITE, COLOR_BLUE);
  //  mvchgat(0, 0, -1, A_BLINK, 1, NULL);

  stats.getStats();
  while(1) {
    attron(COLOR_PAIR(colorPair::border));
    attron(A_BOLD);
    for (int i = 0; i <= 22; ++i) {
      mvprintw(i, 39, " ");
    }
    string version;
    time_t now = time(NULL);
    struct tm *nowtm = localtime(&now);
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", nowtm);
    stats.getStat("version", version);
    mvprintw(0, 0, "         CACHE INFORMATION             ");
    mvprintw(0, 40, "       CLIENT REQUEST & RESPONSE        ");
    mvprintw(16, 0, "             CLIENT                    ");
    mvprintw(16, 40, "           ORIGIN SERVER                ");
    mvprintw(23, 0, "%8.8s - %-10.10s - %-24.24s      (q)uit (h)elp (%c)bsolute  ", timeBuf, version.c_str(), host.c_str(), absolute ? 'A' : 'a');
    attroff(COLOR_PAIR(colorPair::border));
    attroff(A_BOLD);

    list<string> cache1;
    cache1.push_back("disk_used");
    cache1.push_back("disk_total");
    cache1.push_back("ram_used");
    cache1.push_back("ram_total");
    cache1.push_back("lookups");
    cache1.push_back("cache_writes");
    cache1.push_back("cache_updates");
    cache1.push_back("cache_deletes");
    cache1.push_back("read_active");
    cache1.push_back("write_active");
    cache1.push_back("update_active");
    cache1.push_back("entries");
    cache1.push_back("avg_size");
    makeTable(0, 1, cache1, stats);


    list<string> cache2;
    cache2.push_back("ram_ratio");
    cache2.push_back("fresh");
    cache2.push_back("reval");
    cache2.push_back("cold");
    cache2.push_back("changed");
    cache2.push_back("not");
    cache2.push_back("no");
    cache2.push_back("fresh_time");
    cache2.push_back("reval_time");
    cache2.push_back("cold_time");
    cache2.push_back("changed_time");
    cache2.push_back("not_time");
    cache2.push_back("no_time");
    makeTable(21, 1, cache2, stats);

    list<string> response1;
    response1.push_back("get");
    response1.push_back("head");
    response1.push_back("post");
    response1.push_back("2xx");
    response1.push_back("3xx");
    response1.push_back("4xx");
    response1.push_back("5xx");
    response1.push_back("conn_fail");
    response1.push_back("other_err");
    response1.push_back("abort");
    makeTable(41, 1, response1, stats);


    list<string> response2;
    response2.push_back("200");
    response2.push_back("206");
    response2.push_back("301");
    response2.push_back("302");
    response2.push_back("304");
    response2.push_back("404");
    response2.push_back("502");
    response2.push_back("s_100");
    response2.push_back("s_1k");
    response2.push_back("s_3k");
    response2.push_back("s_5k");
    response2.push_back("s_10k");
    response2.push_back("s_1m");
    response2.push_back("s_>1m");
    makeTable(62, 1, response2, stats);

    list<string> client1;
    client1.push_back("client_req");
    client1.push_back("client_req_conn");
    client1.push_back("client_conn");
    client1.push_back("client_curr_conn");
    client1.push_back("client_actv_conn");
    makeTable(0, 17, client1, stats);


    list<string> client2;
    client2.push_back("client_head");
    client2.push_back("client_body");
    client2.push_back("client_avg_size");
    client2.push_back("client_net");
    client2.push_back("client_req_time");
    makeTable(21, 17, client2, stats);

    list<string> server1;
    server1.push_back("server_req");
    server1.push_back("server_req_conn");
    server1.push_back("server_conn");
    server1.push_back("server_curr_conn");
    makeTable(41, 17, server1, stats);


    list<string> server2;
    server2.push_back("server_head");
    server2.push_back("server_body");
    server2.push_back("server_avg_size");
    server2.push_back("server_net");
    makeTable(62, 17, server2, stats);


    curs_set(0);
    refresh();
    timeout(sleep_time);
  loop:
    int x = getch();
    if (x == 'q')
      break;
    if (x == 'h') {
      help(host, version);
    }
    if (x == 'a') {
      absolute = stats.toggleAbsolute();
    }
    if (x == -1)
      stats.getStats();
    clear();
  }
  endwin();

  return 0;
}
