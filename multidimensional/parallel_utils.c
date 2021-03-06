#include <omp.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "parallel_utils.h"

#define ERROR_BUF_SZ 1000
#define DBL_MAX 1.79769e+308
double error_buf[ERROR_BUF_SZ] = {DBL_MAX};

double
single_pertub_error(double* d_current, double* d_goal,
                    double* xs, int row, int pertub_dim,
                    int x_rows, int x_cols, double step)
{
    double error = 0;
    int ll;
    int d_idx = x_rows * row;
    int x_idx = x_cols * row;

    //#pragma omp parallel for reduction (+:error)
    for(ll = 0; ll < x_rows; ll++)
    {
        double d_prev, before, after, diff1, diff;
        if(row != ll)
        {
            d_prev = d_current[d_idx + ll] * d_current[d_idx + ll];
            diff1 = (xs[x_idx + pertub_dim] - xs[ll * x_cols + pertub_dim]);
            before = diff1 * diff1;
            after = (diff1 + step) * (diff1 + step);
            diff = d_goal[d_idx + ll] - sqrt(d_prev - before + after);
            error += diff * diff;
        }
    }
    return error;
}

pertub_res
min_pertub_error_old(double* xs, double radius, double* d_current,
                     double* d_goal, int ii, int x_rows, int x_cols, double percent)
{
    int jj;
    struct pertub_res optimum;
    optimum.error = DBL_MAX;
    double optimum_error = DBL_MAX, optimum_step = radius;
    int optimum_k = 0;

    #pragma omp parallel
    {
        srand((int)time(NULL) ^ omp_get_thread_num());
        int k_local = optimum_k;
        double error_local = optimum_error;
        double step_local = optimum_step;
        #pragma omp for nowait
        for(jj=0; jj < 2 * x_cols; jj++)
        {
            if ((double)rand() / (double)((unsigned)RAND_MAX + 1) > percent)
            {
                continue;
            }
            double step = jj < x_cols ? radius : -radius;
            int kk = jj % x_cols;
            double e = single_pertub_error(
                d_current, d_goal, xs, ii, kk, x_rows, x_cols, step);
            if(e < error_local)
            {
                error_local = e;
                k_local = kk;
                step_local = step;
            }
        }

        #pragma omp critical
        {
            if(error_local < optimum_error)
            {
                optimum_error = error_local;
                optimum_k = k_local;
                optimum_step = step_local;
            }
        }
    }


    optimum.error = optimum_error;
    optimum.k = optimum_k;
    optimum.step = optimum_step;

    return optimum;
}


pertub_res
min_pertub_error(double* xs, double radius, double* d_current,
                 double* d_goal, int ii, int x_rows, int x_cols, double percent)
{
    int jj;
    struct pertub_res optimum;
    optimum.error = DBL_MAX;

    #pragma omp parallel
    {
        srand((int)time(NULL) ^ omp_get_thread_num());
        #pragma omp for nowait
        for(jj=0; jj < 2 * x_cols; jj++)
        {
            if ((double)rand() / (double)((unsigned)RAND_MAX + 1) > percent)
            {
                error_buf[jj] = DBL_MAX;
                continue;
            }
            double step = jj < x_cols ? radius : -radius;
            error_buf[jj] = single_pertub_error(
                d_current, d_goal, xs, ii, jj % x_cols,
                x_rows, x_cols, step);
        }
    }

    for(jj=0; jj < 2 * x_cols; jj++) {
        if(error_buf[jj] < optimum.error) {
            optimum.k = jj % x_cols;
            optimum.step = jj < x_cols ? radius : -radius;
            optimum.error = error_buf[jj];
        }
    }
    return optimum;
}


double
hooke_jeeves(double* xs, double radius, double* d_current,
             double* d_goal, int ii, int x_rows, int x_cols,
              double percent, double curr_error, double error_i)
{
    int jj, ll;
    int x_idx = x_cols * ii;
    int d_idx = x_rows * ii;
  //  #pragma omp parallel
    {
        int l_idx = 0;
        double d_prev = 0;
        double diff1 = 0;
        double before = 0;
        double after = 0;
        double d = 0;

        srand((int)time(NULL) ^ omp_get_thread_num());
//        #pragma omp for nowait
        for(jj=0; jj < 2 * x_cols; jj++)
        {
            if ((double)rand() / (double)((unsigned)RAND_MAX + 1) > percent)
            {
                error_buf[jj] = DBL_MAX;
                continue;
            }
            int k = jj % x_cols;
            double step = jj < x_cols ? radius : -radius;
            double e = single_pertub_error(
                d_current, d_goal, xs, ii, k,
                x_rows, x_cols, step);

            double test_error = curr_error - (error_i - e);
            if(test_error < curr_error) {
                for(ll=0; ll < x_rows; ll++) {
                    if(ii != ll) {
                        l_idx = ll * x_cols;
                        d_prev = d_current[x_rows * ii + ll] * d_current[x_rows * ii + ll];
                        diff1 = (xs[x_cols * ii + k] - xs[x_cols * ll + k]);
                        before = diff1 * diff1;
                        after = (diff1 + step) * (diff1 + step);
                        if(d_prev - before + after < 0) printf("%f\n", d_prev - before + after);
                        d = sqrt(d_prev - before + after);
                        d_current[d_idx + ll] = d;
                        d_current[l_idx + ii] = d;
                    }
                }
                xs[x_idx + k] += step;
                curr_error = test_error;
                error_i = e;
            }
        }
    }

    return curr_error;
}
