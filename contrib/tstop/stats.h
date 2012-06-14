/** @file

    Include file for the tstop stats.

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

#include <curl/curl.h>
#include <map>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <ts/ts.h>
#include <inttypes.h>
#include <ts/mgmtapi.h>

using namespace std;

struct LookupItem {
  LookupItem(const char *s, const char *n, const int t): pretty(s), name(n), type(t) {}
  LookupItem(const char *s, const char *n, const char *d, const int t): pretty(s), name(n), numerator(n), denominator(d), type(t) {}
  const char *pretty;
  const char *name;
  const char *numerator;
  const char *denominator;
  int type;
};
extern size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);
extern char curl_error[CURL_ERROR_SIZE];
extern string response;

namespace constant {
  const char global[] = "\"global\": {\n";
  const char start[] = "\"proxy.process.";
  const char seperator[] = "\": \"";
  const char end[] = "\",\n";
};

//----------------------------------------------------------------------------
class Stats {
public:
  Stats(const string &host) {
    _host = host;
    _stats = NULL;
    _old_stats = NULL;
    _absolute = false;
    lookup_table.insert(make_pair("version", LookupItem("Version", "proxy.process.version.server.short", 1)));
    lookup_table.insert(make_pair("disk_used", LookupItem("Disk Used", "proxy.process.cache.bytes_used", 1)));
    lookup_table.insert(make_pair("disk_total", LookupItem("Disk Total", "proxy.process.cache.bytes_total", 1)));
    lookup_table.insert(make_pair("ram_used", LookupItem("Ram Used", "proxy.process.cache.ram_cache.bytes_used", 1)));
    lookup_table.insert(make_pair("ram_total", LookupItem("Ram Total", "proxy.process.cache.ram_cache.total_bytes", 1)));
    lookup_table.insert(make_pair("lookups", LookupItem("Lookups", "proxy.process.http.cache_lookups", 2)));
    lookup_table.insert(make_pair("cache_writes", LookupItem("Writes", "proxy.process.http.cache_writes", 2)));
    lookup_table.insert(make_pair("cache_updates", LookupItem("Updates", "proxy.process.http.cache_updates", 2)));
    lookup_table.insert(make_pair("cache_deletes", LookupItem("Deletes", "proxy.process.http.cache_deletes", 2)));
    lookup_table.insert(make_pair("read_active", LookupItem("Read Active", "proxy.process.cache.read.active", 1)));
    lookup_table.insert(make_pair("write_active", LookupItem("Writes Active", "proxy.process.cache.write.active", 1)));
    lookup_table.insert(make_pair("update_active", LookupItem("Update Active", "proxy.process.cache.update.active", 1)));
    lookup_table.insert(make_pair("entries", LookupItem("Entries", "proxy.process.cache.direntries.used", 1)));
    lookup_table.insert(make_pair("avg_size", LookupItem("Avg Size", "disk_total", "entries", 3)));

    lookup_table.insert(make_pair("client_req", LookupItem("Requests", "proxy.process.http.incoming_requests", 2)));
    lookup_table.insert(make_pair("client_conn", LookupItem("New Conn", "proxy.process.http.total_client_connections", 2)));
    lookup_table.insert(make_pair("client_req_conn", LookupItem("Req/Conn", "client_req", "client_conn", 3)));
    lookup_table.insert(make_pair("client_curr_conn", LookupItem("Curr Conn", "proxy.process.http.current_client_connections", 1)));
    lookup_table.insert(make_pair("client_actv_conn", LookupItem("Active Con", "proxy.process.http.current_active_client_connections", 1)));

    lookup_table.insert(make_pair("server_req", LookupItem("Requests", "proxy.process.http.outgoing_requests", 2)));
    lookup_table.insert(make_pair("server_conn", LookupItem("New Conn", "proxy.process.http.total_server_connections", 2)));
    lookup_table.insert(make_pair("server_req_conn", LookupItem("Req/Conn", "server_req", "server_conn", 3)));
    lookup_table.insert(make_pair("server_curr_conn", LookupItem("Curr Conn", "proxy.process.http.current_server_connections", 1)));


    lookup_table.insert(make_pair("client_head", LookupItem("Head Bytes", "proxy.process.http.user_agent_response_header_total_size", 2)));
    lookup_table.insert(make_pair("client_body", LookupItem("Body Bytes", "proxy.process.http.user_agent_response_document_total_size", 2)));
    lookup_table.insert(make_pair("server_head", LookupItem("Head Bytes", "proxy.process.http.origin_server_response_header_total_size", 2)));
    lookup_table.insert(make_pair("server_body", LookupItem("Body Bytes", "proxy.process.http.origin_server_response_document_total_size", 2)));

    // not used directly
    lookup_table.insert(make_pair("ram_hit", LookupItem("Ram Hit", "proxy.process.cache.ram_cache.hits", 2)));
    lookup_table.insert(make_pair("ram_miss", LookupItem("Ram Misses", "proxy.process.cache.ram_cache.misses", 2)));



    lookup_table.insert(make_pair("client_abort", LookupItem("Clnt Abort", "proxy.process.http.err_client_abort_count_stat", 2)));
    lookup_table.insert(make_pair("conn_fail", LookupItem("Conn Fail", "proxy.process.http.err_connect_fail_count_stat", 2)));
    lookup_table.insert(make_pair("abort", LookupItem("Abort", "proxy.process.http.transaction_counts.errors.aborts", 2)));
    lookup_table.insert(make_pair("t_conn_fail", LookupItem("Conn Fail", "proxy.process.http.transaction_counts.errors.connect_failed", 2)));
    lookup_table.insert(make_pair("other_err", LookupItem("Other Err", "proxy.process.http.transaction_counts.errors.other", 2)));
    // percentage
    lookup_table.insert(make_pair("ram_ratio", LookupItem("Ram Hit", "ram_hit", "ram_hit_miss", 4)));

    // percetage of requests
    lookup_table.insert(make_pair("fresh", LookupItem("Fresh", "proxy.process.http.transaction_counts.hit_fresh", 5)));
    lookup_table.insert(make_pair("reval", LookupItem("Revalidate", "proxy.process.http.transaction_counts.hit_revalidated", 5)));
    lookup_table.insert(make_pair("cold", LookupItem("Cold", "proxy.process.http.transaction_counts.miss_cold", 5)));
    lookup_table.insert(make_pair("changed", LookupItem("Changed", "proxy.process.http.transaction_counts.miss_changed", 5)));
    lookup_table.insert(make_pair("not", LookupItem("Not Cache", "proxy.process.http.transaction_counts.miss_not_cacheable", 5)));
    lookup_table.insert(make_pair("no", LookupItem("No Cache", "proxy.process.http.transaction_counts.miss_client_no_cache", 5)));

    lookup_table.insert(make_pair("fresh_time", LookupItem("Fresh (ms)", "proxy.process.http.transaction_totaltime.hit_fresh", "fresh", 8)));
    lookup_table.insert(make_pair("reval_time", LookupItem("Reval (ms)", "proxy.process.http.transaction_totaltime.hit_revalidated", "reval", 8)));
    lookup_table.insert(make_pair("cold_time", LookupItem("Cold (ms)", "proxy.process.http.transaction_totaltime.miss_cold", "cold", 8)));
    lookup_table.insert(make_pair("changed_time", LookupItem("Chang (ms)", "proxy.process.http.transaction_totaltime.miss_changed", "changed", 8)));
    lookup_table.insert(make_pair("not_time", LookupItem("Not (ms)", "proxy.process.http.transaction_totaltime.miss_not_cacheable", "not", 8)));
    lookup_table.insert(make_pair("no_time", LookupItem("No (no)", "proxy.process.http.transaction_totaltime.miss_client_no_cache", "no", 8)));

    lookup_table.insert(make_pair("get", LookupItem("GET", "proxy.process.http.get_requests", 5)));
    lookup_table.insert(make_pair("head", LookupItem("HEAD", "proxy.process.http.head_requests", 5)));
    lookup_table.insert(make_pair("post", LookupItem("POST", "proxy.process.http.post_requests", 5)));
    lookup_table.insert(make_pair("2xx", LookupItem("2xx", "proxy.process.http.2xx_responses", 5)));
    lookup_table.insert(make_pair("3xx", LookupItem("3xx", "proxy.process.http.3xx_responses", 5)));
    lookup_table.insert(make_pair("4xx", LookupItem("4xx", "proxy.process.http.4xx_responses", 5)));
    lookup_table.insert(make_pair("5xx", LookupItem("5xx", "proxy.process.http.5xx_responses", 5)));

    lookup_table.insert(make_pair("200", LookupItem("200", "proxy.process.http.200_responses", 5)));
    lookup_table.insert(make_pair("206", LookupItem("206", "proxy.process.http.206_responses", 5)));
    lookup_table.insert(make_pair("301", LookupItem("301", "proxy.process.http.301_responses", 5)));
    lookup_table.insert(make_pair("302", LookupItem("302", "proxy.process.http.302_responses", 5)));
    lookup_table.insert(make_pair("304", LookupItem("304", "proxy.process.http.304_responses", 5)));
    lookup_table.insert(make_pair("404", LookupItem("404", "proxy.process.http.404_responses", 5)));
    lookup_table.insert(make_pair("502", LookupItem("502", "proxy.process.http.502_responses", 5)));

    lookup_table.insert(make_pair("s_100", LookupItem("100 B", "proxy.process.http.response_document_size_100", 5)));
    lookup_table.insert(make_pair("s_1k", LookupItem("1 KB", "proxy.process.http.response_document_size_1K", 5)));
    lookup_table.insert(make_pair("s_3k", LookupItem("3 KB", "proxy.process.http.response_document_size_3K", 5)));
    lookup_table.insert(make_pair("s_5k", LookupItem("5 KB", "proxy.process.http.response_document_size_5K", 5)));
    lookup_table.insert(make_pair("s_10k", LookupItem("10 KB", "proxy.process.http.response_document_size_10K", 5)));
    lookup_table.insert(make_pair("s_1m", LookupItem("1 MB", "proxy.process.http.response_document_size_1M", 5)));
    lookup_table.insert(make_pair("s_>1m", LookupItem("> 1 MB", "proxy.process.http.response_document_size_inf", 5)));


    // sum together
    lookup_table.insert(make_pair("ram_hit_miss", LookupItem("Ram Hit+Miss", "ram_hit", "ram_miss", 6)));
    lookup_table.insert(make_pair("client_net", LookupItem("Net (bits)", "client_head", "client_body", 7)));
    lookup_table.insert(make_pair("client_size", LookupItem("Total Size", "client_head", "client_body", 6)));
    lookup_table.insert(make_pair("client_avg_size", LookupItem("Avg Size", "client_size", "client_req", 3)));

    lookup_table.insert(make_pair("server_net", LookupItem("Net (bits)", "server_head", "server_body", 7)));
    lookup_table.insert(make_pair("server_size", LookupItem("Total Size", "server_head", "server_body", 6)));
    lookup_table.insert(make_pair("server_avg_size", LookupItem("Avg Size", "server_size", "server_req", 3)));


    lookup_table.insert(make_pair("total_time", LookupItem("Total Time", "proxy.process.http.total_transactions_time", 2)));
    lookup_table.insert(make_pair("client_req_time", LookupItem("Resp (ms)", "total_time", "client_req", 3)));
  }

  void getStats() {

    if (_host == "") {
      int64_t value;
      if (_old_stats != NULL) {
        delete _old_stats;
        _old_stats = NULL;
      }
      _old_stats = _stats;
      _stats = new map<string, string>;

      gettimeofday(&_time, NULL);
      double now = _time.tv_sec + (double)_time.tv_usec / 1000000;

      for (map<string, LookupItem>::const_iterator lookup_it = lookup_table.begin();
           lookup_it != lookup_table.end(); ++lookup_it) {
        const LookupItem &item = lookup_it->second;

        if (item.type == 1 || item.type == 2 || item.type == 5 || item.type == 8) {
          if (strcmp(item.pretty, "Version") == 0) {
            // special case for Version informaion
            TSString strValue = NULL;
            assert(TSRecordGetString(item.name, &strValue) == TS_ERR_OKAY);
            string key = item.name;
            (*_stats)[key] = strValue;
          } else {
            assert(TSRecordGetInt(item.name, &value) == TS_ERR_OKAY);
            string key = item.name;
            char buffer[32];
            sprintf(buffer, "%lld", value);
            string foo = buffer;
            (*_stats)[key] = foo;
          }
        }
      } 
      _old_time = _now;
      _now = now;
      _time_diff = _now - _old_time;
    } else {
      CURL *curl;
      CURLcode res;

      curl = curl_easy_init();
      if (curl) {
        string url = "http://" + _host + "/_stats";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);

        // update time
        gettimeofday(&_time, NULL);
        double now = _time.tv_sec + (double)_time.tv_usec / 1000000;

        response.clear();
        response.reserve(32768); // should hopefully be smaller then 32KB
        res = curl_easy_perform(curl);

        // only if success update stats and time information
        if (res == 0) {
          if (_old_stats != NULL) {
            delete _old_stats;
            _old_stats = NULL;
          }
          _old_stats = _stats;
          _stats = new map<string, string>;

          // parse
          parseResponse(response);
          _old_time = _now;
          _now = now;
          _time_diff = _now - _old_time;
        }

        /* always cleanup */ 
        curl_easy_cleanup(curl);
      }
    }
  }

  int64_t getValue(const string &key, const map<string, string> *stats) const {
    map<string, string>::const_iterator stats_it = stats->find(key);
    assert(stats_it != stats->end());
    int64_t value = atoll(stats_it->second.c_str());
    return value;
  }

  void getStat(const string &key, double &value, int overrideType = 0) {
    string strtmp;
    int typetmp;
    getStat(key, value, strtmp, typetmp, overrideType);
  }


  void getStat(const string &key, string &value) {
    map<string, LookupItem>::const_iterator lookup_it = lookup_table.find(key);
    assert(lookup_it != lookup_table.end());
    const LookupItem &item = lookup_it->second;
    
    map<string, string>::const_iterator stats_it = _stats->find(item.name);
    assert(stats_it != _stats->end());
    value = stats_it->second.c_str();
  }

  void getStat(const string &key, double &value, string &prettyName, int &type, int overrideType = 0) {
    map<string, LookupItem>::const_iterator lookup_it = lookup_table.find(key);
    assert(lookup_it != lookup_table.end());
    const LookupItem &item = lookup_it->second;
    prettyName = item.pretty;
    if (overrideType != 0)
      type = overrideType;
    else
      type = item.type;

    if (type == 1 || type == 2 || type == 5 || type == 8) {
      value = getValue(item.name, _stats);
      if (key == "total_time") {
        value = value / 10000000;
      }

      if ((type == 2 || type == 5 || type == 8) && _old_stats != NULL && _absolute == false) {
        double old = getValue(item.name, _old_stats);
        if (key == "total_time") {
          old = old / 10000000;
        }
        value = (value - old) / _time_diff;
      }
    } else if (type == 3 || type == 4) {
      double numerator;
      double denominator;
      getStat(item.numerator, numerator);
      getStat(item.denominator, denominator);
      if (denominator == 0)
        value = 0;
      else
        value = numerator / denominator;
      if (type == 4) {
        value *= 100;
      }
    } else if (type == 6 || type == 7) {
      double numerator;
      double denominator;
      getStat(item.numerator, numerator, 2);
      getStat(item.denominator, denominator, 2);
      value = numerator + denominator;
      if (type == 7)
        value *= 8;
    }

    if (type == 8) {
      double denominator;
      getStat(item.denominator, denominator, 2);
      if (denominator == 0)
        value = 0;
      else
        value = value / denominator * 1000;
    }

    if (type == 5) {
      double denominator;
      getStat("client_req", denominator);
      value = value / denominator * 100;
    }


  }

  bool toggleAbsolute() {
    if (_absolute == true)
      _absolute = false;
    else
      _absolute = true;

    return _absolute;
  }

  void parseResponse(const string &response) {
    // move past global
    size_t pos = response.find(constant::global);
    pos += sizeof(constant::global) - 1;

    // find parts of the line
    while (1) {
      size_t start = response.find(constant::start, pos);
      size_t seperator = response.find(constant::seperator, pos);
      size_t end = response.find(constant::end, pos);
  
      if (start == string::npos || seperator == string::npos || end == string::npos)
        return;

      //cout << constant::start << " " << start << endl;
      //cout << constant::seperator << " " << seperator << endl;
      //cout << constant::end << " " << end << endl;

      string key = response.substr(start + 1, seperator - start - 1);
      string value = response.substr(seperator + sizeof(constant::seperator) - 1, end - seperator - sizeof(constant::seperator) + 1);

      (*_stats)[key] = value;
      //cout << "key " << key << " " << "value " << value << endl;
      pos = end + sizeof(constant::end) - 1;
      //cout << "pos: " << pos << endl;
    }
  }

  ~Stats() {
    if (_stats != NULL) {
      delete _stats;
    }
    if (_old_stats != NULL) {
      delete _old_stats;
    }
  }

private:
  map<string, string> *_stats;
  map<string, string> *_old_stats;
  map<string, LookupItem> lookup_table;
  string _host;
  double _old_time;
  double _now;
  double _time_diff;
  struct timeval _time;
  bool _absolute;
};
