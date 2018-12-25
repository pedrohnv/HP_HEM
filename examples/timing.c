/* check timing for both 20pwrd02grcev and 51emc03grcev
for each integration type */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cubature.h>
#include <Electrode.h>
#include <auxiliary.h>
//#include <omp.h>
#include <time.h>

int impedances_single(
    Electrode* electrodes, int num_electrodes, _Complex double* zl,
    _Complex double* zt, _Complex double gamma, _Complex double s, double mu,
    _Complex double kappa, _Complex double* mpot)
{
    double ls, lr, k1, k2, cost, rbar;
    _Complex double iwu_4pi = s*mu/(FOUR_PI);
    _Complex double one_4pik = 1.0/(FOUR_PI*kappa);
    _Complex double intg;
    int i, k, m;
    // _self and _mutual impedances are not used to reduce the number of
    // operations, as some of them would be done twice or more unnecessarily
    for (i = 0; i < num_electrodes; i++)
    {
        for (k = i; k < num_electrodes; k++)
        {
            ls = electrodes[i].length;
            lr = electrodes[k].length;
            if (i == k)
            {
                k1 = electrodes[i].radius/ls;
                k2 = sqrt(1.0 + k1*k1);
                cost = 2.0*(log( (k2 + 1.)/k1 ) - k2 + k1);
                zl[i*num_electrodes + k] = iwu_4pi*ls*cost + electrodes[i].zi;
                zt[i*num_electrodes + k] = one_4pik/ls*cost;
            }
            else
            {
                cost = 0.0;
                for (m = 0; m < 3; m++)
                {
                    k1 = (electrodes[i].end_point[m] - electrodes[i].start_point[m]);
                    k2 = (electrodes[k].end_point[m] - electrodes[k].start_point[m]);
                    cost += k1*k2;
                    rbar = pow(electrodes[k].middle_point[m]
                        - electrodes[i].middle_point[m], 2.0);
                }
                rbar = sqrt(rbar);
                cost = cost/(ls*lr);
                intg = cexp(-gamma*rbar)*mpot[i*num_electrodes + k];
                zl[i*num_electrodes + k] = iwu_4pi*intg*cost;
                zt[i*num_electrodes + k] = one_4pik/(ls*lr)*intg;

                zl[k*num_electrodes + i] = zl[i*num_electrodes + k];
                zt[k*num_electrodes + i] = zt[i*num_electrodes + k];
            }
        }
    }
    return 0;
}

int run_20pwrd02grcev(double length, double rho, char file_name[],
    int intg_type)
{
    // parameters
    double h = 0.001; //burial depth
    double r = 1.25e-2; //radius
    double sigma = 1/rho; // soil conductivity
    double er = 10.0; //soil rel. permitivitty
    double rho_c = 1.9e-6; //copper resistivity
    //char file_name[] = "20pwrd02grcev_L<>rho<>.dat";
    // frequencies of interest
    int nf = 250;
    double freq[nf];
    double start_point[3] = {0., 0., -h};
    double end_point[3] = {0., 0., -h - length};

    remove(file_name);
    FILE* save_file = fopen(file_name, "w");
    if (save_file == NULL)
    {
        printf("Cannot open file %s\n",  file_name);
        exit(1);
    }
    logspace(2, 7, nf, freq);

    // electrode definition and segmentation
    double lambda = wave_length(freq[nf - 1], sigma, er*EPS0, MU0); //smallest
    int num_electrodes = ceil( length/(lambda/6.0) ) ;
    int num_nodes = num_electrodes + 1;
    double nodes[num_nodes][3];
    Electrode* electrodes = (Electrode*) malloc(sizeof(Electrode)*num_electrodes);
    // the internal impedance is added "outside" later
    segment_electrode(
        electrodes, nodes, num_electrodes, start_point, end_point, r, 0.0);

    // create images
    start_point[2] = h;
    end_point[2] = h + length;
    double nodes_images[num_nodes][3];
    //Electrode images[num_electrodes];
    Electrode* images = (Electrode*) malloc(sizeof(Electrode)*num_electrodes);
    segment_electrode(
        images, nodes_images, num_electrodes, start_point, end_point, r, 0.0);

    //build system to be solved
    int ne2 = num_electrodes*num_electrodes;
    int nn2 = num_nodes*num_nodes;
    int ss1 = (2*num_electrodes + num_nodes);
    int ss2 = ss1*ss1;
    _Complex double s;
    _Complex double kappa, gamma, zinternal;
    _Complex double* mpot = (_Complex double*) malloc(sizeof(_Complex double)*ne2);
    _Complex double* zl = (_Complex double*) malloc(sizeof(_Complex double)*ne2);
    _Complex double* zt = (_Complex double*) malloc(sizeof(_Complex double)*ne2);
    _Complex double* yn = (_Complex double*) malloc(sizeof(_Complex double)*nn2);
    _Complex double* ie = (_Complex double*) malloc(sizeof(_Complex double)*ss1);
    _Complex double* ie_cp = (_Complex double*) malloc(sizeof(_Complex double)*ss1);
    _Complex double* we = (_Complex double*) malloc(sizeof(_Complex double)*ss2);
    _Complex double* we_cp = (_Complex double*) malloc(sizeof(_Complex double)*ss2);
    int i, k;
    for (i = 0; i < nn2; i++)
    {
        yn[i] = 0.0; //external nodal admittance
    }
    for (i = 0; i < ss1; i++)
    {
        ie[i] = 0.0;
    }
    ie[ss1 - num_nodes] = 1.0;
    fill_incidence(we, electrodes, num_electrodes, nodes, num_nodes);
    if (intg_type == INTG_LOGNF)
    {
        calculate_impedances(
            electrodes, num_electrodes, mpot, zt, 1.0, 1.0, 1.0, 1.0,
            200, 1e-3, 1e-4, ERROR_PAIRED, intg_type);
    }
    // solve for each frequency: WE*VE = IE
    for (i = 0; i < nf; i++)
    {
        s = I*TWO_PI*freq[i];
        kappa = (sigma + s*er*EPS0); //soil complex conductivity
        gamma = csqrt(s*MU0*kappa); //soil propagation constant
        //TODO especialized impedance calculation taking advantage of symmetry
        if (intg_type == INTG_LOGNF)
        {
            impedances_single(
                electrodes, num_electrodes, zl, zt, gamma, s, MU0, kappa, mpot);
        } else {
            calculate_impedances(
                electrodes, num_electrodes, zl, zt, gamma, s, MU0, kappa,
                200, 1e-3, 1e-4, ERROR_PAIRED, intg_type);
        }
        zinternal = internal_impedance(s, rho_c, r, MU0)*electrodes[0].length;
        for (k = 0; k < num_electrodes; k++)
        {
            zl[k*num_electrodes] += zinternal;
        }
        impedances_images(electrodes, images, num_electrodes, zl, zt, gamma,
            s, MU0, kappa, 0.0, 1.0, 200, 1e-3, 1e-4, ERROR_PAIRED, intg_type);
        fill_impedance(we, electrodes, num_electrodes, num_nodes, zl, zt, yn);
        //The matrices are pivoted in-place. To recover them, copy
        copy_array(we, we_cp, ss2);
        copy_array(ie, ie_cp, ss1);
        solve_electrodes(we_cp, ie_cp, num_electrodes, num_nodes);
        fprintf(save_file, "%f %f\n",
                creal(ie_cp[ss1 - num_nodes]), cimag(ie_cp[ss1 - num_nodes]));
    }
    fclose(save_file);
    free(electrodes);
    free(images);
    free(zl);
    free(zt);
    free(yn);
    free(ie);
    free(ie_cp);
    free(we);
    free(we_cp);
    free(mpot);
    return 0;
}

int run_51emc03grcev(double length, double rho, char file_name[], int intg_type)
{
    // parameters
    double h = 0.8; //burial depth
    double r = 7e-3; //radius
    double sigma = 1/rho; // soil conductivity
    double er = 10.0; //soil rel. permitivitty
    double rho_c = 1.9e-6; //copper resistivity
    //char file_name[] = "20pwrd02grcev_L<>rho<>.dat";
    // frequencies of interest
    int nf = 250;
    double freq[nf];
    double start_point[3] = {0., 0., -h};
    double end_point[3] = {length, 0., -h};

    remove(file_name);
    FILE* save_file = fopen(file_name, "w");
    if (save_file == NULL)
    {
        printf("Cannot open file %s\n",  file_name);
        exit(1);
    }
    logspace(2, 7, nf, freq);

    // electrode definition and segmentation
    double lambda = wave_length(freq[nf - 1], sigma, er*EPS0, MU0); //smallest
    int num_electrodes = ceil( length/(lambda/6.0) ) ;
    int num_nodes = num_electrodes + 1;
    double nodes[num_nodes][3];
    Electrode* electrodes = (Electrode*) malloc(sizeof(Electrode)*num_electrodes);
    // the internal impedance is added "outside" later
    segment_electrode(
        electrodes, nodes, num_electrodes, start_point, end_point, r, 0.0);

    // create images
    start_point[2] = h;
    end_point[2] = h;
    double nodes_images[num_nodes][3];
    //Electrode images[num_electrodes];
    Electrode* images = (Electrode*) malloc(sizeof(Electrode)*num_electrodes);
    segment_electrode(
        images, nodes_images, num_electrodes, start_point, end_point, r, 0.0);

    //build system to be solved
    int ne2 = num_electrodes*num_electrodes;
    int nn2 = num_nodes*num_nodes;
    int ss1 = (2*num_electrodes + num_nodes);
    int ss2 = ss1*ss1;
    _Complex double s;
    _Complex double kappa, gamma, zinternal;
    _Complex double* mpot = (_Complex double*) malloc(sizeof(_Complex double)*ne2);
    _Complex double* zl = (_Complex double*) malloc(sizeof(_Complex double)*ne2);
    _Complex double* zt = (_Complex double*) malloc(sizeof(_Complex double)*ne2);
    _Complex double* yn = (_Complex double*) malloc(sizeof(_Complex double)*nn2);
    _Complex double* ie = (_Complex double*) malloc(sizeof(_Complex double)*ss1);
    _Complex double* ie_cp = (_Complex double*) malloc(sizeof(_Complex double)*ss1);
    _Complex double* we = (_Complex double*) malloc(sizeof(_Complex double)*ss2);
    _Complex double* we_cp = (_Complex double*) malloc(sizeof(_Complex double)*ss2);
    int i, k;
    for (i = 0; i < nn2; i++)
    {
        yn[i] = 0.0; //external nodal admittance
    }
    for (i = 0; i < ss1; i++)
    {
        ie[i] = 0.0;
    }
    ie[ss1 - num_nodes] = 1.0;
    fill_incidence(we, electrodes, num_electrodes, nodes, num_nodes);
    if (intg_type == INTG_LOGNF)
    {
        calculate_impedances(
            electrodes, num_electrodes, mpot, zt, 1.0, 1.0, 1.0, 1.0,
            200, 1e-3, 1e-4, ERROR_PAIRED, intg_type);
    }
    // solve for each frequency: WE*VE = IE
    for (i = 0; i < nf; i++)
    {
        s = I*TWO_PI*freq[i];
        kappa = (sigma + s*er*EPS0); //soil complex conductivity
        gamma = csqrt(s*MU0*kappa); //soil propagation constant
        //TODO especialized impedance calculation taking advantage of symmetry
        if (intg_type == INTG_LOGNF)
        {
            impedances_single(
                electrodes, num_electrodes, zl, zt, gamma, s, MU0, kappa, mpot);
        } else {
            calculate_impedances(
                electrodes, num_electrodes, zl, zt, gamma, s, MU0, kappa,
                200, 1e-3, 1e-4, ERROR_PAIRED, intg_type);
        }
        zinternal = internal_impedance(w, rho_c, r, MU0)*electrodes[0].length;
        for (k = 0; k < num_electrodes; k++)
        {
            zl[k*num_electrodes] += zinternal;
        }
        impedances_images(electrodes, images, num_electrodes, zl, zt, gamma,
            s, MU0, kappa, 0.0, 1.0, 200, 1e-3, 1e-4, ERROR_PAIRED, intg_type);
        fill_impedance(we, electrodes, num_electrodes, num_nodes, zl, zt, yn);
        //The matrices are pivoted in-place. To recover them, copy
        copy_array(we, we_cp, ss2);
        copy_array(ie, ie_cp, ss1);
        solve_electrodes(we_cp, ie_cp, num_electrodes, num_nodes);
        fprintf(save_file, "%f %f\n",
                creal(ie_cp[ss1 - num_nodes]), cimag(ie_cp[ss1 - num_nodes]));
    }
    fclose(save_file);
    free(electrodes);
    free(images);
    free(zl);
    free(zt);
    free(yn);
    free(ie);
    free(ie_cp);
    free(we);
    free(we_cp);
    free(mpot);
    return 0;
}

int run_timing(int intg_type, char *file_name, int loops)
{
    remove(file_name);
    FILE* save_file = fopen(file_name, "w");
    if (save_file == NULL)
    {
        printf("Cannot open file %s\n",  file_name);
        exit(1);
    }
    int i;
    clock_t begin, end, full_begin = clock();
    double time_spent;
    printf("test case 51emc03grcev\n=== START ===\n");
    // ============================================================
    fprintf(save_file, "51emc03grcev_L10rho10\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_51emc03grcev(10.0, 10.0, "examples/51emc03grcev_L10rho10.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "51emc03grcev_L10rho100\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_51emc03grcev(10.0, 100.0, "examples/51emc03grcev_L10rho100.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "51emc03grcev_L10rho1000\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_51emc03grcev(10.0, 1000.0, "examples/51emc03grcev_L10rho1000.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    fprintf(save_file, "51emc03grcev_L100rho10\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_51emc03grcev(100.0, 10.0, "examples/51emc03grcev_L100rho10.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "51emc03grcev_L100rho100\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_51emc03grcev(100.0, 100.0, "examples/51emc03grcev_L100rho100.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "51emc03grcev_L100rho1000\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_51emc03grcev(100.0, 1000.0, "examples/51emc03grcev_L100rho1000.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    printf("test case 20pwrd02grcev\n=== START ===\n");
    // ============================================================
    fprintf(save_file, "20pwrd02grcev_L3rho10\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_20pwrd02grcev(3.0, 10.0, "examples/20pwrd02grcev_L3rho10.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "20pwrd02grcev_L3rho100\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_20pwrd02grcev(3.0, 100.0, "examples/20pwrd02grcev_L3rho100.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "20pwrd02grcev_L3rho1000\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_20pwrd02grcev(3.0, 1000.0, "examples/20pwrd02grcev_L3rho1000.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "20pwrd02grcev_L24rho10\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_20pwrd02grcev(24.0, 10.0, "examples/20pwrd02grcev_L24rho10.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "20pwrd02grcev_L24rho100\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_20pwrd02grcev(24.0, 100.0, "examples/20pwrd02grcev_L24rho100.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    fprintf(save_file, "20pwrd02grcev_L24rho1000\n");
    for (i = 0; i < loops; i++)
    {
        begin = clock();
        run_20pwrd02grcev(24.0, 1000.0, "examples/20pwrd02grcev_L24rho1000.dat", intg_type);
        end = clock();
        time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
        fprintf(save_file, "%f\n", time_spent);
    }
    // ============================================================
    printf("==== END ====\n");
    fclose(save_file);
    end = clock();
    time_spent = (double) (end - full_begin)/CLOCKS_PER_SEC;
    printf("total CPU time spent: %f\n", time_spent);
    return 0;
}

int main(int argc, char *argv[])
{
    run_timing(INTG_DOUBLE, "timing_intg_double.dat", 1);
    run_timing(INTG_LOGNF, "timing_intg_single.dat", 1);
}
