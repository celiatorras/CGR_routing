/*
 * example_next_hop.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <stdbool.h>

#include "../include/UniboCGR.h"

#define LINE_MAX 1000

// Minimun callback for backlog (UniboCGR_open needs a callback)
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
        printf("no contacts registered o error get_first_contact\n");
        return;
    }
    printf("Contacts:\n");
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

static int load_contact_plan_from_file(UniboCGR cgr, const char *filename, time_t now) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open contact plan file '%s': %s\n", filename, strerror(errno));
        return -1;
    }

    char line[LINE_MAX];
    int lineno = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        lineno++;
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r' || line[l-1] == ' ' || line[l-1] == '\t')) {
            line[--l] = '\0';
        }
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;

        char *save = NULL;
        char *token = strtok_r(p, ",", &save);
        if (!token) continue;

        if (strcmp(token, "CONTACT") == 0) {
            char *s_from = strtok_r(NULL, ",", &save);
            char *s_to = strtok_r(NULL, ",", &save);
            char *s_start = strtok_r(NULL, ",", &save);
            char *s_end = strtok_r(NULL, ",", &save);
            char *s_xmit = strtok_r(NULL, ",", &save);
            char *s_conf = strtok_r(NULL, ",", &save);
            char *s_mtv_bulk = strtok_r(NULL, ",", &save);
            char *s_mtv_normal = strtok_r(NULL, ",", &save);
            char *s_mtv_expedited = strtok_r(NULL, ",", &save);

            if (!s_from || !s_to || !s_start || !s_end || !s_xmit || !s_conf || !s_mtv_bulk || !s_mtv_normal || !s_mtv_expedited) {
                fprintf(stderr, "Parse error in %s:%d (CONTACT) -> not enough fields\n", filename, lineno);
                fclose(f);
                return -2;
            }

            uint64_t from = strtoull(s_from, NULL, 10);
            uint64_t to = strtoull(s_to, NULL, 10);
            long start = strtol(s_start, NULL, 10);
            long end = strtol(s_end, NULL, 10);
            uint64_t xmit = strtoull(s_xmit, NULL, 10);
            float conf = strtof(s_conf, NULL);
            double mtv_bulk = strtod(s_mtv_bulk, NULL);
            double mtv_normal = strtod(s_mtv_normal, NULL);
            double mtv_expedited = strtod(s_mtv_expedited, NULL);

            UniboCGR_Contact c;
            UniboCGR_Error rc = UniboCGR_Contact_create(&c);
            if (rc != UniboCGR_NoError) {
                fprintf(stderr, "Failed to create contact object (line %d)\n", lineno);
                fclose(f);
                return -2;
            }
            UniboCGR_Contact_set_sender(c, from);
            UniboCGR_Contact_set_receiver(c, to);
            UniboCGR_Contact_set_start_time(cgr, c, (time_t)start + now);
            UniboCGR_Contact_set_end_time(cgr, c, (time_t)end + now);
            UniboCGR_Contact_set_xmit_rate(c, (int)xmit);
            UniboCGR_Contact_set_confidence(c, conf);
            UniboCGR_Contact_set_mtv_bulk(c, mtv_bulk);
            UniboCGR_Contact_set_mtv_normal(c, mtv_normal);
            UniboCGR_Contact_set_mtv_expedited(c, mtv_expedited);

            rc = UniboCGR_contact_plan_add_contact(cgr, c, true);
            if (rc != UniboCGR_NoError) {
                fprintf(stderr, "Failed to add contact (line %d): %s\n", lineno, UniboCGR_get_error_string(rc));
                UniboCGR_Contact_destroy(&c);
                fclose(f);
                return -2;
            }
            UniboCGR_Contact_destroy(&c);

        } else if (strcmp(token, "RANGE") == 0) {
            char *s_from = strtok_r(NULL, ",", &save);
            char *s_to = strtok_r(NULL, ",", &save);
            char *s_start = strtok_r(NULL, ",", &save);
            char *s_end = strtok_r(NULL, ",", &save);

            if (!s_from || !s_to || !s_start || !s_end) {
                fprintf(stderr, "Parse error in %s:%d (RANGE) -> not enough fields\n", filename, lineno);
                fclose(f);
                return -2;
            }

            uint64_t from = strtoull(s_from, NULL, 10);
            uint64_t to = strtoull(s_to, NULL, 10);
            long start = strtol(s_start, NULL, 10);
            long end = strtol(s_end, NULL, 10);

            UniboCGR_Range r;
            UniboCGR_Error rc = UniboCGR_Range_create(&r);
            if (rc != UniboCGR_NoError) {
                fprintf(stderr, "Failed to create range object (line %d)\n", lineno);
                fclose(f);
                return -2;
            }
            UniboCGR_Range_set_sender(r, from);
            UniboCGR_Range_set_receiver(r, to);
            UniboCGR_Range_set_start_time(cgr, r, (time_t)start + now);
            UniboCGR_Range_set_end_time(cgr, r, (time_t)end + now);
            
            rc = UniboCGR_contact_plan_add_range(cgr, r);

            if (rc != UniboCGR_NoError) {
                fprintf(stderr, "Failed to add range (line %d): %s\n", lineno, UniboCGR_get_error_string(rc));
                UniboCGR_Range_destroy(&r);
                fclose(f);
                return -2;
            }
            UniboCGR_Range_destroy(&r);

        } else {
            fprintf(stderr, "Unknown record type '%s' in %s:%d -> ignored\n", token, filename, lineno);
            continue;
        }
    }

    fclose(f);
    return 0;
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

    int err = load_contact_plan_from_file(cgr, "contact_plan.txt", now);
    if (err != 0) {
        fprintf(stderr, "Failed to load contact plan file (err=%d)\n", err);
    }

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

    //excluded neighbors list is null
    UniboCGR_excluded_neighbors_list excluded = NULL;
    rc = UniboCGR_create_excluded_neighbors_list(&excluded);
    check_and_exit_if_error(rc, "UniboCGR_create_excluded_neighbors_list");

    UniboCGR_route_list route_list = NULL;
    rc = UniboCGR_routing(cgr, bundle, excluded, &route_list);
    if (rc == UniboCGR_ErrorRouteNotFound) {
        fprintf(stderr, "There's no route to destination.\n");

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
        fprintf(stderr, "Could not open the first route %s\n", UniboCGR_get_error_string(rc));
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
