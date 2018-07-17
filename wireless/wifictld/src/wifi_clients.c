#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <libubox/avl.h>
#include "hostapd/ieee802_11_defs.h" // ETH_ALEN
#include "log.h"
#include "wifi_clients.h"

int client_try_threashold = 4;
int client_signal_threashold = -75;

struct avl_tree clients_by_addr;

static int avl_compare_macaddr(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, ETH_ALEN);
}

int wifi_clients_init() {
	avl_init(&clients_by_addr, avl_compare_macaddr, false, NULL);
	return 0;
}

void wifi_clients_close() {

}

struct wifi_client *__get_client(const u8 *address){
	struct wifi_client *client;

	client = avl_find_element(&clients_by_addr, address, client, avl);
	if (client) {
		log_debug("wifi_clients.__get_client("MACSTR"): found existing client\n", MAC2STR(address));
		return client;
	}
	log_debug("wifi_clients.__get_client("MACSTR"): gen new client\n", MAC2STR(address));
	client = calloc(sizeof(*client), 1);
	memcpy(client->addr, address, sizeof(client->addr));
	client->try = 0;
	client->connected = 0;
	client->freq_highest = 0;
	client->signal_lowfreq = 0;
	client->signal_highfreq = 0;
	client->time = 0;
	client->avl.key = client->addr;
	log_debug("wifi_clients.__get_client("MACSTR"): add client to mem\n", MAC2STR(address));
	avl_insert(&clients_by_addr, &client->avl);
	return client;
}

void __client_learn(struct wifi_client *client, uint32_t freq, uint32_t ssi_signal) {
	log_debug("wifi_clients.__client_learn(., %d):", freq);
	if (client->freq_highest < freq) {
		client->freq_highest = freq;
		log_debug(" new highest freq");
	}
	if (freq > WIFI_CLIENT_FREQ_THREASHOLD) {
		client->signal_highfreq = ssi_signal;
	}else{
		client->signal_lowfreq = ssi_signal;
	}
	log_debug("\n");
	//TODO time set and reset clean
}

void wifi_clients_learn(const u8 *address, uint32_t freq, uint32_t ssi_signal) {
	struct wifi_client *client;
	client = __get_client(address);
	__client_learn(client, freq, ssi_signal);
}

int wifi_clients_try(const u8 *address, uint32_t freq, uint32_t ssi_signal) {
	struct wifi_client *client;
	client = __get_client(address);
	__client_learn(client, freq, ssi_signal);

	client->try++;

	log_info("auth(mac="MACSTR" freq=%d ssi=%d try=%d): ", MAC2STR(address), freq, ssi_signal, client->try);
	if (freq > WIFI_CLIENT_FREQ_THREASHOLD) {
		log_info("accept\n");
		client->try = 0;
		client->connected = 1;
		return 0;
	}
	//TODO handle ssi_signal
	if (client->freq_highest > WIFI_CLIENT_FREQ_THREASHOLD &&
		ssi_signal > client_signal_threashold
		) {
		log_info("reject - learned higher freq\n");
		return -1;
	}
	if(client->try > client_try_threashold) {
		log_info("accept - threashold\n");
		client->try = 0;
		client->connected = 1;
		return 0;
	}
	log_info("reject\n");
	return client->try;
}

void wifi_clients_disconnect(const u8 *address, uint32_t freq, uint32_t ssi_signal) {
	struct wifi_client *client;
	client = __get_client(address);
	__client_learn(client, freq, ssi_signal);
	client->connected = 0;
}

void wifi_clients_del(const u8 *addr) {
	struct wifi_client *client;

	client = avl_find_element(&clients_by_addr, addr, client, avl);
	if (!client)
			return;

	avl_delete(&clients_by_addr, &client->avl);
	free(client);
}
