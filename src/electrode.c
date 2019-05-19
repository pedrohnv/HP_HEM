#include <math.h>
#include <float.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <omp.h>
#include "electrode.h"
#include "auxiliary.h"
#include "cubature.h"

int
populate_electrode (Electrode *electrode, const double start_point[3],
                    const double end_point[3], double radius,
                    _Complex double internal_impedance)
{
    if (equal_points(start_point, end_point) == 1) {
        printf("Error: start_point and end_point are equal.\n");
        return 1;
    }
    if (radius <= 0) {
        printf("Error: radius < 0.\n");
        return 2;
    }
    electrode->radius = radius;
    electrode->zi = internal_impedance;
    electrode->length = vector_norm(start_point, end_point);
    for (size_t i = 0; i < 3; i++) {
        electrode->start_point[i] = start_point[i];
        electrode->end_point[i] = end_point[i];
        electrode->middle_point[i] = (start_point[i] + end_point[i])/2.0;
    }
    return 0;
}

int
electrodes_file (const char file_name[], Electrode *electrodes,
                 size_t num_electrodes)
{
    FILE *stream = fopen(file_name, "r");
    if (stream == NULL) {
        printf("Cannot open file %s\n", file_name);
        exit(1);
    }
    double start_point[3], end_point[3];
    double radius, rezi, imzi;
    int success = 9;
    int pop_error = 0;
    for (size_t i = 0; i < num_electrodes; i++) {
        success = fscanf(stream, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
                         &start_point[0], &start_point[1], &start_point[2],
                         &end_point[0], &end_point[1], &end_point[2],
                         &radius, &rezi, &imzi);
        if (success != 9) {
            printf("error reading line %i of file %s\n", (int) (i+1), file_name);
            break;
        }
        success = populate_electrode(&(electrodes[i]), start_point, end_point,
                                     radius, rezi + I*imzi);
        if (success != 0) {
            printf("Bad input: could not create electrode %i from file %s\n",
                   (int) (i+1), file_name);
            pop_error = success;
            break;
        }
    }
    fclose(stream);
    if (pop_error == 0) {
        return (success - 9);
    } else {
        return (pop_error);
    }
}

int
electrode_grid (Electrode *electrodes, double *nodes, size_t num_nodes,
                double radius, double depth, _Complex double zi, int v1,
                double length1, int l1, int v2, double length2, int l2)
{
    // TODO argument checking
    double h = depth;
    int num_electrodes = l1*v2*(v1 - 1) + l2*v1*(v2 - 1);
    Electrode *edges = malloc(num_electrodes * sizeof(Electrode));
    double len1 = length1/(v1 - 1);
    double len2 = length2/(v2 - 1);
    int n = 0;
    for (int i = 0; i < v2; i++) {
        for (int k = 0; k < v1; k++) {
            nodes[n*3] = k*len1;
            nodes[n*3 + 1] = i*len2;
            nodes[n*3 + 2] = h;
            n++;
        }
    }
    double xx[v1];
    linspace(0.0, length1, v1, xx);
    double yy[v2];
    linspace(0.0, length2, v2, yy);
    int e = 0;
    double start_point[3] = {0.0, 0.0, h};
    double end_point[3] = {0.0, 0.0, h};
    for (int k = 0; k < v2; k++) {
        start_point[1] = yy[k];
        end_point[1] = yy[k];
        for (int i = 0; i < (v1 - 1); i++) {
            start_point[0] = xx[i];
            end_point[0] = xx[i+1];
            populate_electrode(&(edges[e]), start_point, end_point, radius, zi);
            e++;
        }
    }
    for (int k = 0; k < v1; k++) {
        start_point[0] = xx[k];
        end_point[0] = xx[k];
        for (int i = 0; i < (v2 - 1); i++) {
            start_point[1] = yy[i];
            end_point[1] = yy[i+1];
            populate_electrode(&(edges[e]), start_point, end_point, radius, zi);
            e++;
        }
    }
    // Now divide the edges...
    e = 0;
    int s = 0;
    double lx, ly;
    for (int i = 0; i < v1*(v2 - 1); i++) {
        lx = (edges[e].end_point[0] - edges[e].start_point[0])/l1;
        ly = (edges[e].end_point[1] - edges[e].start_point[1])/l1;
        for (int k = 0; k < l1; k++) {
            start_point[0] = edges[e].start_point[0] + k*lx;
            start_point[1] = edges[e].start_point[1] + k*ly;
            end_point[0] = edges[e].start_point[0] + (k + 1)*lx;
            end_point[1] = edges[e].start_point[1] + (k + 1)*ly;
            populate_electrode(&(electrodes[s]), start_point, end_point, radius, zi);
            s++;
            if (k > 0) {
                nodes[n*3] = start_point[0];
                nodes[n*3 + 1] = start_point[1];
                nodes[n*3 + 2] = h;
                n++;
            }
        }
        e++;
    }
    for (int i = 0; i < v2*(v1 - 1); i++) {
        lx = (edges[e].end_point[0] - edges[e].start_point[0])/l2;
        ly = (edges[e].end_point[1] - edges[e].start_point[1])/l2;
        for (int k = 0; k < l2; k++) {
            start_point[0] = edges[e].start_point[0] + k*lx;
            start_point[1] = edges[e].start_point[1] + k*ly;
            end_point[0] = edges[e].start_point[0] + (k + 1)*lx;
            end_point[1] = edges[e].start_point[1] + (k + 1)*ly;
            populate_electrode(&(electrodes[s]), start_point, end_point, radius, zi);
            s++;
            if (k > 0) {
                nodes[n*3] = start_point[0];
                nodes[n*3 + 1] = start_point[1];
                nodes[n*3 + 2] = h;
                n++;
            }
        }
        e++;
    }
    free(edges);
    return 0;
}

int
nodes_file (const char file_name[], double *nodes, size_t num_nodes)
{
    FILE *stream = fopen(file_name, "r");
    if (stream == NULL) {
        printf("Cannot open file %s\n", file_name);
        exit(1);
    }
    int success = 3;
    for (size_t i = 0; i < num_nodes; i++) {
        success = fscanf(stream, "%lf %lf %lf", &nodes[i*3],
                         &nodes[i*3 + 1], &nodes[i*3 + 2]);
        if (success != 3) {
            printf("error reading line %i of file %s\n", (int) (i+1), file_name);
            break;
        }
    }
    fclose(stream);
    return (success - 3);
}

int
segment_electrode (Electrode *electrodes, double *nodes, size_t num_segments,
                   const double *start_point, const double *end_point,
                   double radius, _Complex double unit_zi)
{
    if (num_segments < 1) {
        printf("Error: number of segments should be greater than 0.\n");
        return 1;
    }
    size_t num_nodes = num_segments + 1;
    double x[num_nodes];
    double y[num_nodes];
    double z[num_nodes];
    linspace(start_point[0], end_point[0], num_nodes, x);
    linspace(start_point[1], end_point[1], num_nodes, y);
    linspace(start_point[2], end_point[2], num_nodes, z);
    for (size_t i = 0; i < num_nodes; i++) {
        nodes[i*3] = x[i];
        nodes[i*3 + 1] = y[i];
        nodes[i*3 + 2] = z[i];
    }
    double total_length = vector_norm(start_point, end_point);
    _Complex double zi = unit_zi*total_length/num_segments;
    for (size_t i = 0; i < num_segments; i++) {
        populate_electrode(&(electrodes[i]), (nodes + i*3), (nodes + i*3 + 3),
                           radius, zi);
    }
    return 0;
}

size_t
nodes_from_elecs (double *nodes, Electrode *electrodes, size_t num_electrodes)
{
    // Array of pointers hat will have the maximum number of nodes possible.
    // Make them NULL and malloc for each new unique node.
    // Later copy them to nodes
    double **dummy_nodes = malloc(2*num_electrodes * sizeof(double[3]));
    for (size_t i = 1; i < 2*num_electrodes; i++) {
        dummy_nodes[i] = NULL;
    }
    size_t num_nodes = 0;
    int node_present;
    for (size_t i = 0; i < num_electrodes; i++) {
        // start_point in nodes?
        node_present = 0;
        for (size_t k = 0; k < num_nodes; k++) {
            if (equal_points(electrodes[i].start_point, dummy_nodes[k])) {
                node_present = 1;
                break;
            }
        }
        if (!node_present) {
            dummy_nodes[num_nodes] = electrodes[i].start_point;
            num_nodes++;
        }
        // end_point in nodes?
        node_present = 0;
        for (size_t k = 0; k < num_nodes; k++) {
            if (equal_points(electrodes[i].end_point, dummy_nodes[k])) {
                node_present = 1;
                break;
            }
        }
        if (!node_present) {
            dummy_nodes[num_nodes] = electrodes[i].end_point;
            num_nodes++;
        }
    }
    for (size_t i = 0; i < num_nodes; i++) {
        for (size_t k = 0; k < 3; k++) {
            nodes[i*3 + k] = dummy_nodes[i][k];
        }
    }
    free(dummy_nodes);
    return num_nodes;
}

int
integrand_double (unsigned ndim, const double *t, void *auxdata, unsigned fdim,
                  double *fval)
{
    Integration_data *p = (Integration_data*) auxdata;
    const Electrode *sender = p->sender;
    const Electrode *receiver = p->receiver;
    _Complex double gamma = p->gamma;
    double point_r, point_s, r = 0.0;
    for (size_t i = 0; i < 3; i++) {
        point_r = t[0]*(receiver->end_point[i] - receiver->start_point[i]);
        point_r += receiver->start_point[i];

        point_s = t[1]*(sender->end_point[i] - sender->start_point[i]);
        point_s += sender->start_point[i];

        r += pow(point_r - point_s, 2.0);
    }
    r = sqrt(r);
    _Complex double exp_gr = cexp(-gamma*r);
    fval[0] = creal(exp_gr)/r;
    fval[1] = cimag(exp_gr)/r;
    return 0;
}

int
exp_logNf (unsigned ndim, const double *t, void *auxdata, unsigned fdim,
           double *fval)
{
    Integration_data *p = (Integration_data*) auxdata;
    const Electrode *sender = p->sender;
    const Electrode *receiver = p->receiver;
    double r1, r2, eta, point_r;
    r1 = 0.0;
    r2 = 0.0;
    eta = 0.0;
    for (size_t i = 0; i < 3; i++) {
        point_r = t[0]*(receiver->end_point[i] - receiver->start_point[i]);
        point_r += receiver->start_point[i];
        r1 += pow(point_r - sender->start_point[i], 2.0);
        r2 += pow(point_r - sender->end_point[i], 2.0);
        eta += pow(point_r - sender->middle_point[i], 2.0);
    }
    r1 = sqrt(r1);
    r2 = sqrt(r2);
    eta = sqrt(eta);
    _Complex double exp_gr;
    if (p->simplified) {
        exp_gr = 1.0;
    } else {
        exp_gr = cexp(- p->gamma*eta);
    }
    double Nf = (r1 + r2 + sender->length)/(r1 + r2 - sender->length);
    exp_gr = exp_gr*log(Nf);
    fval[0] = creal(exp_gr);
    fval[1] = cimag(exp_gr);
    return 0;
}

int
integral (const Electrode *sender, const Electrode *receiver,
          _Complex double gamma, size_t max_eval, double req_abs_error,
          double req_rel_error, int error_norm, int integration_type,
          double result[2], double error[2])
{
    Integration_data *auxdata = malloc(sizeof(Integration_data));
    auxdata->sender = sender;
    auxdata->receiver = receiver;
    auxdata->gamma = gamma;
    double tmin[] = {0.0, 0.0};
    double tmax[] = {1.0, 1.0};
    int failure = 1;
    double rbar, lslr;
    _Complex double exp_gr;
    switch (integration_type) {
        case INTG_NONE:
            lslr = (sender->length)*(receiver->length);
            rbar = vector_norm(sender->middle_point, receiver->middle_point);
            exp_gr = cexp(-gamma*rbar)/rbar*lslr;
            result[0] = creal(exp_gr);
            result[1] = cimag(exp_gr);
            failure = 0;
            break;
        case INTG_DOUBLE:
            failure = hcubature(2, integrand_double, auxdata, 2, tmin, tmax,
                                max_eval, req_abs_error, req_rel_error,
                                error_norm, result, error);
            result[0] = result[0] * receiver->length * sender->length;
            result[1] = result[1] * receiver->length * sender->length;
            break;
        case INTG_EXP_LOGNF:
            auxdata->simplified = 0;
            failure = hcubature(2, exp_logNf, auxdata, 1, tmin, tmax, max_eval,
                                req_abs_error, req_rel_error, error_norm,
                                result, error);
            result[0] = result[0] * receiver->length;
            result[1] = result[1] * receiver->length;
            break;
        case INTG_LOGNF:
            auxdata->simplified = 1;
            failure = hcubature(1, exp_logNf, auxdata, 1, tmin, tmax, max_eval,
                                req_abs_error, req_rel_error, error_norm,
                                result, error);
            rbar = vector_norm(sender->middle_point, receiver->middle_point);
            exp_gr = cexp(-gamma*rbar);
            result[0] = creal(exp_gr) * receiver->length;
            result[1] = cimag(exp_gr) * receiver->length;
            break;
        default:
            failure = -10;
            break;
    }
    free(auxdata);
    return failure;
}

_Complex double
internal_impedance (_Complex double s, double rho, double radius, double mur)
{
    /*_Complex double etapr = csqrt(s*mur*MU0/rho);
    _Complex double etapr_radius = etapr*radius;
    double zr = creal(etapr_radius);
    double zi = cimag(etapr_radius);
    double fnu0 = 0;
    double fnu1 = 1;
    int kode = 1;
    int n = 1;
    double cyr0, cyi0, cyr1, cyi1;
    int nz, ierr;
    zbesi_(&zr, &zi, &fnu0, &kode, &n, &cyr0, &cyi0, &nz, &ierr);
    if (ierr != 0) {
        printf("in function 'internal_impedance'\n");
        printf("error in call to zbesi(fnu = 0)\n");
        printf("error flag = %i\n", ierr);
        exit(ierr);
    }
    zbesi_(&zr, &zi, &fnu1, &kode, &n, &cyr1, &cyi1, &nz, &ierr);
    if (ierr != 0) {
        printf("in function 'internal_impedance'\n");
        printf("error in call to zbesi(fnu = 1)\n");
        printf("error flag = %i\n", ierr);
        exit(ierr);
    }
    return (etapr*rho*(cyr0 + cyi0*I))/(TWO_PI*radius*(cyr1 + cyi1*I));
    FIXME leave out for now...*/
    return (0.0 + 0.0*I);
}

// Longitudinal impedance
_Complex double
longitudinal_self (const Electrode *electrode, _Complex double s, double mur)
{
    double ls = electrode->length;
    double k1 = electrode->radius/ls;
    double k2 = sqrt(1.0 + k1*k1);
    _Complex double zi = electrode->zi;
    return s*mur*MU0*ls/(TWO_PI)*(log( (k2 + 1.)/k1 ) - k2 + k1) + zi;
}

_Complex double
longitudinal_mutual (const Electrode *sender, const Electrode *receiver,
                     _Complex double s, double mur, _Complex double gamma,
                     size_t max_eval, double req_abs_error, double req_rel_error,
                     int error_norm, int integration_type, double result[2],
                     double error[2])
{
    double cost = 0.0; //cosine between segments
    double k1, k2;
    for (size_t i = 0; i < 3; i++) {
        k1 = (sender->end_point[i] - sender->start_point[i]);
        k2 = (receiver->end_point[i] - receiver->start_point[i]);
        cost += k1*k2;
    }
    cost = abs(cost/(sender->length * receiver->length));
    if (fabs(cost - 0.0) < DBL_EPSILON) {
        return 0.0;
    } else {
        integral(sender, receiver, gamma, max_eval, req_abs_error,
                 req_rel_error, error_norm, integration_type, result, error);
        _Complex double intg = result[0] + I*result[1];
        return s*mur*MU0/(FOUR_PI)*intg*cost;
    }
}

// Transveral impedance
_Complex double
transversal_self (const Electrode *electrode, _Complex double kappa)
{
    double ls = electrode->length;
    double k1 = electrode->radius/ls;
    double k2 = sqrt(1.0 + k1*k1);
    return 1.0/(TWO_PI*kappa*ls)*(log( (k2 + 1.)/k1 ) - k2 + k1);
}

_Complex double
transversal_mutual (const Electrode *sender, const Electrode *receiver,
                    _Complex double kappa, _Complex double gamma, size_t max_eval,
                    double req_abs_error, double req_rel_error, int error_norm,
                    int integration_type, double result[2], double error[2])
{
    double ls = sender->length;
    double lr = receiver->length;
    integral(sender, receiver, gamma, max_eval, req_abs_error,
             req_rel_error, error_norm, integration_type, result, error);
    _Complex double intg = result[0] + I*result[1];
    return 1.0/(FOUR_PI*kappa*ls*lr)*intg;
}

// TODO store ZT and ZL as upper/lower triangular matrices, as they are symmetric
// TODO use COLUMN_MAJOR ordering to avoid having transpose the matrices for LAPACK
int
calculate_impedances (const Electrode *electrodes, size_t num_electrodes,
                      _Complex double *zl, _Complex double *zt,
                      _Complex double gamma, _Complex double s, double mur,
                      _Complex double kappa, size_t max_eval, double req_abs_error,
                      double req_rel_error, int error_norm, int integration_type)
{
    double result[2], error[2], ls, lr, k1[3], k2, cost;
    _Complex double iwu_4pi = s*mur*MU0/(FOUR_PI);
    _Complex double one_4pik = 1.0/(FOUR_PI*kappa);
    _Complex double intg;
    int failure;
    // _self and _mutual impedances are not used to reduce the number of
    // operations, as some of them would be done twice or more unnecessarily
    for (size_t i = 0; i < num_electrodes; i++) {
        ls = electrodes[i].length;
        k1[0] = electrodes[i].radius/ls;
        k2 = sqrt(1.0 + k1[0]*k1[0]);
        cost = 2.0*(log( (k2 + 1.)/k1[0] ) - k2 + k1[0]);
        zl[i*num_electrodes + i] = iwu_4pi*ls*cost + electrodes[i].zi;
        zt[i*num_electrodes + i] = one_4pik/ls*cost;
        for (size_t m = 0; m < 3; m++) {
            k1[m] = (electrodes[i].end_point[m] - electrodes[i].start_point[m]);
        }
        for (size_t k = i+1; k < num_electrodes; k++) {
            lr = electrodes[k].length;
            cost = 0.0;
            for (size_t m = 0; m < 3; m++) {
                k2 = (electrodes[k].end_point[m] - electrodes[k].start_point[m]);
                cost += k1[m]*k2;
            }
            cost = abs(cost/(ls*lr));
            failure = integral(&(electrodes[i]), &(electrodes[k]), gamma,
                               max_eval, req_abs_error, req_rel_error,
                               error_norm, integration_type, result, error);
            if (failure) return failure;
            intg = result[0] + I*result[1];
            zl[i*num_electrodes + k] = iwu_4pi*intg*cost;
            zt[i*num_electrodes + k] = one_4pik/(ls*lr)*intg;

            zl[k*num_electrodes + i] = zl[i*num_electrodes + k];
            zt[k*num_electrodes + i] = zt[i*num_electrodes + k];
        }
    }
    return 0;
}

int
impedances_images (const Electrode *electrodes, const Electrode *images,
                   size_t num_electrodes, _Complex double *zl, _Complex double *zt,
                   _Complex double gamma, _Complex double s, double mur,
                   _Complex double kappa, _Complex double ref_l,
                   _Complex double ref_t, size_t max_eval, double req_abs_error,
                   double req_rel_error, int error_norm, int integration_type)
{
    double result[2], error[2], ls, lr, k1[3], k2, cost;
    _Complex double iwu_4pi = s*mur*MU0/(FOUR_PI);
    _Complex double one_4pik = 1.0/(FOUR_PI*kappa);
    _Complex double intg;
    int failure;
    // _mutual impedances are not used to reduce the number of
    // operations, as some of them would be done twice or more unnecessarily
    for (size_t i = 0; i < num_electrodes; i++) {
        ls = electrodes[i].length;
        for (size_t m = 0; m < 3; m++) {
            k1[m] = (electrodes[i].end_point[m] - electrodes[i].start_point[m]);
        }
        for (size_t k = i; k < num_electrodes; k++) {
            lr = images[k].length;
            cost = 0.0;
            for (size_t m = 0; m < 3; m++) {
                k2 = (images[k].end_point[m] - images[k].start_point[m]);
                cost += k1[m]*k2;
            }
            cost = abs(cost/(ls*lr));
            failure = integral(&(electrodes[i]), &(images[k]), gamma, max_eval,
                               req_abs_error, req_rel_error, error_norm,
                               integration_type, result, error);
            if (failure) return failure;
            intg = result[0] + I*result[1];
            zl[i*num_electrodes + k] += ref_l*iwu_4pi*intg*cost;
            zt[i*num_electrodes + k] += ref_t*one_4pik/(ls*lr)*intg;

            zl[k*num_electrodes + i] = zl[i*num_electrodes + k];
            zt[k*num_electrodes + i] = zt[i*num_electrodes + k];
        }
    }
    return 0;
}
