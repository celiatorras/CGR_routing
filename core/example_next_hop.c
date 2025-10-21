/*
 * example_next_hop.c
 *
 * Exemple d'ús mínim d'Unibo-CGR per obtenir el next hop d'un bundle.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <stdbool.h>

#include "../include/UniboCGR.h"

/* ------- Callback mínim per backlog (la UniboCGR_open exigeix un callback) ------- */
/* Signatura segons l'API: retorna int, plenes els punters d'applicable i total backlog (en uint64_t). */
static int my_compute_applicable_backlog(uint64_t neighbor,
                                         UniboCGR_BundlePriority priority,
                                         uint8_t ordinal,
                                         uint64_t *applicableBacklog,
                                         uint64_t *totalBacklog,
                                         void *userArg)
{
    (void) neighbor; (void) priority; (void) ordinal; (void) userArg;
    if (applicableBacklog) *applicableBacklog = 0ULL;  /* aquí pots consultar la cua local per veí */
    if (totalBacklog)      *totalBacklog      = 0ULL;
    return 0; 
}

static void check_and_exit_if_error(UniboCGR_Error rc, const char *ctx) {
    if (rc != UniboCGR_NoError) {
        fprintf(stderr, "Error [%s]: %s\n", ctx, UniboCGR_get_error_string(rc));
        exit(EXIT_FAILURE);
    }
}

static void print_all_contacts(UniboCGR cgr) {
    UniboCGR_Contact ct;
    UniboCGR_Error rc = UniboCGR_get_first_contact(cgr, &ct);
    if (rc != UniboCGR_NoError) {
        printf("no hi ha contactes registrats o error get_first_contact\n");
        return;
    }
    printf("Contactes registrats:\n");
    do {
        uint64_t s = UniboCGR_Contact_get_sender(ct);
        uint64_t r = UniboCGR_Contact_get_receiver(ct);
        time_t start = UniboCGR_Contact_get_start_time(cgr, ct);
        time_t end = UniboCGR_Contact_get_end_time(cgr, ct);
        int xmit = UniboCGR_Contact_get_xmit_rate(ct);
        float c = UniboCGR_Contact_get_confidence(ct);
        printf("  %llu -> %llu   start=%ld  end=%ld   xmit=%u   conf=%.2f\n", (unsigned long long)s, (unsigned long long)r, (long)start, (long)end, xmit, c);

        rc = UniboCGR_get_next_contact(cgr, &ct);
    } while (rc == UniboCGR_NoError);
}

int main(void)
{
    time_t now = time(NULL);
    time_t reference_time = 0; //current_time = now - reference_time -> 0
    uint64_t local_node = 1;

    UniboCGR cgr = NULL;
    UniboCGR_Error rc;

    rc = UniboCGR_open(&cgr, now, reference_time, local_node, PhaseThreeCostFunction_default, my_compute_applicable_backlog, NULL);                          
    check_and_exit_if_error(rc, "UniboCGR_open");

    rc = UniboCGR_contact_plan_open(cgr, now);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_open");

    //Contact 1 -> 2 
    UniboCGR_Contact c1;
    rc = UniboCGR_Contact_create(&c1);
    check_and_exit_if_error(rc, "UniboCGR_Contact_create c1");
    UniboCGR_Contact_set_sender(c1, 1);
    UniboCGR_Contact_set_receiver(c1, 2);
    UniboCGR_Contact_set_start_time(cgr, c1, now); //in seconds
    UniboCGR_Contact_set_end_time(cgr, c1, now + 100000);
    UniboCGR_Contact_set_xmit_rate(c1, 1000); // 1000 bytes/s
    UniboCGR_Contact_set_confidence(c1, 1.0f);
    UniboCGR_Contact_set_mtv_normal(c1, 1024.0 * 1024.0);
    UniboCGR_Contact_set_mtv_bulk(c1,   1024.0 * 1024.0);
    UniboCGR_Contact_set_mtv_expedited(c1, 1024.0 * 1024.0);

    rc = UniboCGR_contact_plan_add_contact(cgr, c1, true);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_add_contact c1");
    UniboCGR_Contact_destroy(&c1);

    //Contact 2 -> 3
    UniboCGR_Contact c2;
    rc = UniboCGR_Contact_create(&c2);
    check_and_exit_if_error(rc, "UniboCGR_Contact_create c2");
    UniboCGR_Contact_set_sender(c2, 2);
    UniboCGR_Contact_set_receiver(c2, 3);
    UniboCGR_Contact_set_start_time(cgr, c2, now);
    UniboCGR_Contact_set_end_time(cgr, c2, now + 100000);
    UniboCGR_Contact_set_xmit_rate(c2, 1000);
    UniboCGR_Contact_set_confidence(c2, 1.0f);
    UniboCGR_Contact_set_mtv_normal(c2, 1024.0 * 1024.0);
    UniboCGR_Contact_set_mtv_bulk(c2,   1024.0 * 1024.0);
    UniboCGR_Contact_set_mtv_expedited(c2, 1024.0 * 1024.0);
    
    rc = UniboCGR_contact_plan_add_contact(cgr, c2, true);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_add_contact c2");
    UniboCGR_Contact_destroy(&c2);
    
    /* --- RANGES --- */
    UniboCGR_Range r1;
    rc = UniboCGR_Range_create(&r1);
    check_and_exit_if_error(rc, "UniboCGR_Range_create r1");
    UniboCGR_Range_set_sender(r1, 1);
    UniboCGR_Range_set_receiver(r1, 2);
    UniboCGR_Range_set_start_time(cgr, r1, now);
    UniboCGR_Range_set_end_time(cgr, r1, now + 100000);
    UniboCGR_Range_set_one_way_light_time(r1, 1); /* 1 second OWLT */
    rc = UniboCGR_contact_plan_add_range(cgr, r1);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_add_range r1");
    UniboCGR_Range_destroy(&r1);

    UniboCGR_Range r2;
    rc = UniboCGR_Range_create(&r2);
    check_and_exit_if_error(rc, "UniboCGR_Range_create r2");
    UniboCGR_Range_set_sender(r2, 2);
    UniboCGR_Range_set_receiver(r2, 3);
    UniboCGR_Range_set_start_time(cgr, r2, now);
    UniboCGR_Range_set_end_time(cgr, r2, now + 100000);
    UniboCGR_Range_set_one_way_light_time(r2, 1); /* 1 second OWLT */
    rc = UniboCGR_contact_plan_add_range(cgr, r2);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_add_range r2");
    UniboCGR_Range_destroy(&r2);
    /* ---------------------------- */

    print_all_contacts(cgr); //contacts print

    rc = UniboCGR_contact_plan_close(cgr);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_close");

    //enable logger
    rc = UniboCGR_feature_open(cgr, now);
    check_and_exit_if_error(rc, "UniboCGR_feature_open");

    rc = UniboCGR_feature_logger_enable(cgr, "/tmp/unibo_logs");
    check_and_exit_if_error(rc, "UniboCGR_feature_logger_enable");

    rc = UniboCGR_feature_close(cgr);
    check_and_exit_if_error(rc, "UniboCGR_feature_close");

    rc = UniboCGR_routing_open(cgr, now);
    check_and_exit_if_error(rc, "UniboCGR_routing_open");

    UniboCGR_Bundle bundle; 
    rc = UniboCGR_Bundle_create(&bundle);
    check_and_exit_if_error(rc, "UniboCGR_Bundle_create");

    UniboCGR_Bundle_set_destination_node_id(bundle, 3); 
    UniboCGR_Bundle_set_creation_time(bundle, (uint64_t) now * 1000); //in milliseconds
    UniboCGR_Bundle_set_lifetime(bundle, 10000000000); //in milliseconds
    UniboCGR_Bundle_set_bundle_protocol_version(bundle, 7);
    UniboCGR_Bundle_set_payload_length(bundle, 500); /* bytes */
    UniboCGR_Bundle_set_total_application_data_unit_length(bundle, 500);

    //excluded neighbors list is null
    UniboCGR_excluded_neighbors_list excluded = NULL;
    rc = UniboCGR_create_excluded_neighbors_list(&excluded);
    check_and_exit_if_error(rc, "UniboCGR_create_excluded_neighbors_list");

    UniboCGR_route_list route_list = NULL;
    rc = UniboCGR_routing(cgr, bundle, excluded, &route_list);
    if (rc == UniboCGR_ErrorRouteNotFound) {
        fprintf(stderr, "No s'ha trobat ruta cap al destí.\n");

        UniboCGR_destroy_excluded_neighbors_list(&excluded);
        UniboCGR_Bundle_destroy(&bundle);
        UniboCGR_routing_close(cgr);
        UniboCGR_close(&cgr, now);
        return EXIT_FAILURE;
    }
    check_and_exit_if_error(rc, "UniboCGR_routing");


    UniboCGR_Route first_route;
    rc = UniboCGR_get_first_route(cgr, route_list, &first_route);
    if (rc != UniboCGR_NoError) {
        fprintf(stderr, "No s'ha pogut obtenir la primera ruta: %s\n", UniboCGR_get_error_string(rc));
    } else {
        uint64_t next_hop = UniboCGR_Route_get_neighbor(first_route);
        printf("Next hop: %" PRIu64 "\n", next_hop);
    }

    UniboCGR_destroy_excluded_neighbors_list(&excluded);
    UniboCGR_Bundle_destroy(&bundle);

    rc = UniboCGR_routing_close(cgr);
    check_and_exit_if_error(rc, "UniboCGR_routing_close");

    UniboCGR_close(&cgr, now);

    return EXIT_SUCCESS;
}
