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
        printf("  %llu -> %llu   start=%ld  end=%ld   \n", (unsigned long long)s, (unsigned long long)r, (long)start, (long)end);

        rc = UniboCGR_get_next_contact(cgr, &ct);
    } while (rc == UniboCGR_NoError);
}

int main(void)
{
    time_t now = time(NULL);
    time_t reference_time = 0; //current_time = now - reference_time -> posem 0
    uint64_t local_node = 1;

    UniboCGR cgr = NULL;
    UniboCGR_Error rc;

    /* 2) Obrim la instància Unibo-CGR */
    rc = UniboCGR_open(&cgr,
                       now,
                       reference_time,
                       local_node,
                       PhaseThreeCostFunction_default, /* cost function per fase 3: la per defecte */
                       my_compute_applicable_backlog,  /* callback backlog */
                       NULL);                           /* userArg per callback */
    check_and_exit_if_error(rc, "UniboCGR_open");

    /* ---------- Contact plan: afegim un parell de contactes perquè hi hagi camí ---------- */
    rc = UniboCGR_contact_plan_open(cgr, now);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_open");

    //Exemple: nodes: 1 (local) -> 2 -> 3 (destí)

    //Contacte 1 -> 2 
    UniboCGR_Contact c1;
    rc = UniboCGR_Contact_create(&c1);
    check_and_exit_if_error(rc, "UniboCGR_Contact_create c1");
    UniboCGR_Contact_set_sender(c1, 1);
    UniboCGR_Contact_set_receiver(c1, 2);
    UniboCGR_Contact_set_start_time(cgr, c1, now); //en segons
    UniboCGR_Contact_set_end_time(cgr, c1, now + 100000);
    rc = UniboCGR_contact_plan_add_contact(cgr, c1, true);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_add_contact c1");
    UniboCGR_Contact_destroy(&c1);

    //Contacte 2 -> 3
    UniboCGR_Contact c2;
    rc = UniboCGR_Contact_create(&c2);
    check_and_exit_if_error(rc, "UniboCGR_Contact_create c2");
    UniboCGR_Contact_set_sender(c2, 2);
    UniboCGR_Contact_set_receiver(c2, 3);
    UniboCGR_Contact_set_start_time(cgr, c2, now); //en segons
    UniboCGR_Contact_set_end_time(cgr, c2, now + 100000);
    rc = UniboCGR_contact_plan_add_contact(cgr, c2, true);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_add_contact c2");
    UniboCGR_Contact_destroy(&c2);
    
    print_all_contacts(cgr); //fem un print per comprovar que s'han creat correctament els contactes a cgr

    rc = UniboCGR_contact_plan_close(cgr);
    check_and_exit_if_error(rc, "UniboCGR_contact_plan_close");

    /* ---------- Preparem el bundle i cridem routing ---------- */
    rc = UniboCGR_routing_open(cgr, now);
    check_and_exit_if_error(rc, "UniboCGR_routing_open");

    /* 3) Creem un bundle i li posem el destí = 3 */
    UniboCGR_Bundle bundle;
    rc = UniboCGR_Bundle_create(&bundle);
    check_and_exit_if_error(rc, "UniboCGR_Bundle_create");

    UniboCGR_Bundle_set_destination_node_id(bundle, 3); 
    UniboCGR_Bundle_set_payload_length(bundle, 100000ULL);
    UniboCGR_Bundle_set_total_application_data_unit_length(bundle, 100000ULL);
    UniboCGR_Bundle_set_creation_time(bundle, (uint64_t) now * 1000ULL);
    UniboCGR_Bundle_set_lifetime(bundle, 3600000ULL);
    UniboCGR_Bundle_set_bundle_protocol_version(bundle, 7);

    /* 4) Construïm la llista d'excluded neighbors (buida per ara) */
    UniboCGR_excluded_neighbors_list excluded = NULL;
    rc = UniboCGR_create_excluded_neighbors_list(&excluded);
    check_and_exit_if_error(rc, "UniboCGR_create_excluded_neighbors_list");

    /* 5) Cridem UniboCGR_routing per obtenir la route_list */
    UniboCGR_route_list route_list = NULL;
    rc = UniboCGR_routing(cgr, bundle, excluded, &route_list);
    if (rc == UniboCGR_ErrorRouteNotFound) {
        fprintf(stderr, "No s'ha trobat ruta cap al destí.\n");
        /* Neteja parcial i sortida neta */
        UniboCGR_destroy_excluded_neighbors_list(&excluded);
        UniboCGR_Bundle_destroy(&bundle);
        UniboCGR_routing_close(cgr);
        UniboCGR_close(&cgr, now);
        return EXIT_FAILURE;
    }
    check_and_exit_if_error(rc, "UniboCGR_routing");

    /* 6) Extraiem la primera ruta i el neighbor (next hop) */
    UniboCGR_Route first_route;
    rc = UniboCGR_get_first_route(cgr, route_list, &first_route);
    if (rc != UniboCGR_NoError) {
        fprintf(stderr, "No s'ha pogut obtenir la primera ruta: %s\n", UniboCGR_get_error_string(rc));
    } else {
        uint64_t next_hop = UniboCGR_Route_get_neighbor(first_route);
        printf("Next hop calculat per Unibo-CGR: %" PRIu64 "\n", next_hop);

        /* Opcional: també pots llegir més propietats de la ruta */
        float arrival_conf = UniboCGR_Route_get_arrival_confidence(first_route);
        time_t best_arrival = UniboCGR_Route_get_best_case_arrival_time(cgr, first_route);
        printf("  arrival_confidence=%f, best_case_arrival_time=%ld (unix)\n",
               arrival_conf, (long) best_arrival);
    }

    
    /* 8) Alliberem recursos que hem creat */
    UniboCGR_destroy_excluded_neighbors_list(&excluded);
    UniboCGR_Bundle_destroy(&bundle);

    /* Tanquem sessió routing i la instància */
    rc = UniboCGR_routing_close(cgr);
    check_and_exit_if_error(rc, "UniboCGR_routing_close");

    UniboCGR_close(&cgr, now);

    return EXIT_SUCCESS;
}
