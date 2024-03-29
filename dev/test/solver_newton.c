#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <lapacke.h>

/*
#####################################################
    CONSTANTS
#####################################################
*/
const double ZERO_ERROR = 1e-8;
const double PI = 3.14159265359;
const double FACTOR = 1 / (4 * PI);
const int LAYERS = 300;
const double CTAU_CRIT = 1e-1;
const int LAMINAR_FLOW = 0;

/*
#####################################################
    STRUCTS
#####################################################
*/
struct Point
{
    double x;
    double y;
    double z;
};

struct VerticeConnection
{
    int n;
    double *coeffs;
    int *faces;
};

struct FacesConnection
{
    int n;
    int *faces;
};

struct ProfileParameters
{
    int n;
    double *eta;
    double *U;
    double *W;
    double *dU_deta;
    double *dW_deta;
    double *S;
    double *T;
    double *R;
};

struct FreestreamParameters
{
    double velocity;
    double density;
    double viscosity;
    double mach;
};

struct IntegralThicknessParameters
{
    double delta_1_ast;
    double delta_2_ast;
    double phi_11;
    double phi_12;
    double phi_21;
    double phi_22;
    double phi_1_ast;
    double phi_2_ast;
    double delta_1_line;
    double delta_2_line;
    double delta_q;
    double delta_q_o;
    double theta_1_o;
    double theta_2_o;
    double delta_1_o;
    double delta_2_o;
    double C_D;
    double C_D_x;
    double C_D_o;
    double C_f_1;
    double C_f_2;
    double theta_11;
    double theta_22;
};

struct IntegralDefectParameters
{
    double M_x;
    double M_y;
    double J_xx;
    double J_xy;
    double J_yx;
    double J_yy;
    double E_x;
    double E_y;
    double K_o_x;
    double K_o_y;
    double Q_x;
    double Q_y;
    double Q_o_x;
    double Q_o_y;
    double tau_w_x;
    double tau_w_y;
    double D;
    double D_x;
    double D_o;
    double K_tau_xx;
    double K_tau_xy;
    double K_tau_yx;
    double K_tau_yy;
    double S_tau_x;
    double S_tau_y;
};

struct EquationsParameters
{
    double M_x;
    double M_y;
    double J_xx;
    double J_xy;
    double J_yx;
    double J_yy;
    double E_x;
    double E_y;
    double K_o_x;
    double K_o_y;
    double Q_x;
    double Q_y;
    double Q_o_x;
    double Q_o_y;
    double tau_w_x;
    double tau_w_y;
    double D;
    double D_x;
    double D_o;
    double K_tau_xx;
    double K_tau_xy;
    double K_tau_yx;
    double K_tau_yy;
    double S_tau_x;
    double S_tau_y;
    double grad_q2_x, grad_q2_y;
    double grad_phi_x, grad_phi_y;
    double div_M;
    double div_J_x;
    double div_J_y;
    double div_E;
    double div_K_o;
    double div_K_tau_x;
    double div_K_tau_y;
    double vel;
    double density;
};

struct DivergentParameters
{
    double M;
    double J_x;
    double J_y;
    double E;
    double K_o;
    double K_tau_x;
    double K_tau_y;
};

struct GradientParameters
{
    double q2_x;
    double q2_y;
    double phi_x;
    double phi_y;
};

struct BoundaryLayerEquations
{
    double momentum_x;
    double momentum_y;
    double kinetic_energy;
    double lateral_curvature;
    double shear_stress_x;
    double shear_stress_y;
};

/*
#####################################################
    MATH FUNCTIONS
#####################################################
*/
double division(double a,
                double b)
{
    if ((-ZERO_ERROR < b) && (b < 0))
    {
        return -a / ZERO_ERROR;
    }
    else if ((0 < b) && (b < ZERO_ERROR))
    {
        return a / ZERO_ERROR;
    }
    else
    {
        return a / b;
    }
}

double norm(struct Point p)
{
    return sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
}

struct Point cross(struct Point p1,
                   struct Point p2)
{
    struct Point p = {p1.y * p2.z - p1.z * p2.y, p1.z * p2.x - p1.x * p2.z, p1.x * p2.y - p1.y * p2.x};
    return p;
}

double dot(struct Point p1,
           struct Point p2)
{
    return p1.x * p2.x + p1.y * p2.y + p1.z * p2.z;
}

double angleBetweenVectors(struct Point p1,
                           struct Point p2)
{

    double norm1 = norm(p1);
    double norm2 = norm(p2);
    double dot12 = dot(p1, p2);

    return acos(dot12 / (norm1 * norm2));
}

double absValue(double a)
{
    if (a < 0)
    {
        return -a;
    }
    else
    {
        return a;
    }
}

void integrate_trap(int n,
                    double *x,
                    double *y,
                    double *out,
                    double mult)
{

    int i;

    *out = 0.0;

    for (i = 0; i < n - 1; i++)
    {
        *out = *out + 0.5 * (x[i + 1] - x[i]) * (y[i + 1] + y[i]);
    }

    *out = *out * mult;
}

/*
#####################################################
    HELPER FUNCTIONS
#####################################################
*/
void calculateVerticesConnection(int nv,
                                 int nf,
                                 double *vertices,
                                 int *faces,
                                 struct VerticeConnection *connection)
{

    /* Parameters */
    int i, j, k, n, faceLine, verticeLine1, verticeLine2, verticeLine3;
    int *facesIds;
    double *angles;
    struct Point point1, point2;
    double angle;
    double sum;

    /* Initialize */
    facesIds = (int *)malloc(50 * sizeof(int));
    angles = (double *)malloc(50 * sizeof(double));

    /* Loop over vertices */
    for (i = 0; i < nv; i++)
    {

        // Reset the number of faces and angles
        n = 0;
        sum = 0.0;

        /* Loop over faces */
        for (j = 0; j < nf; j++)
        {

            faceLine = j * 3;

            /* Check if the face contain the vertice */
            if ((faces[faceLine] == i) || (faces[faceLine + 1] == i) || (faces[faceLine + 2] == i))
            {

                /* Calculate the angle */
                if (faces[faceLine] == i)
                {
                    verticeLine1 = 3 * faces[faceLine];
                    verticeLine2 = 3 * faces[faceLine + 1];
                    verticeLine3 = 3 * faces[faceLine + 2];
                }
                else if (faces[faceLine + 1] == i)
                {
                    verticeLine3 = 3 * faces[faceLine];
                    verticeLine1 = 3 * faces[faceLine + 1];
                    verticeLine2 = 3 * faces[faceLine + 2];
                }
                else
                {
                    verticeLine2 = 3 * faces[faceLine];
                    verticeLine3 = 3 * faces[faceLine + 1];
                    verticeLine1 = 3 * faces[faceLine + 2];
                }

                point1.x = vertices[verticeLine2] - vertices[verticeLine1];
                point1.y = vertices[verticeLine2 + 1] - vertices[verticeLine1 + 1];
                point1.z = vertices[verticeLine2 + 2] - vertices[verticeLine1 + 2];

                point2.x = vertices[verticeLine3] - vertices[verticeLine1];
                point2.y = vertices[verticeLine3 + 1] - vertices[verticeLine1 + 1];
                point2.z = vertices[verticeLine3 + 2] - vertices[verticeLine1 + 2];

                angle = angleBetweenVectors(point1, point2);

                /* Increase the number of faces and save the angle */
                angles[n] = angle;
                facesIds[n] = j;
                sum = sum + angle;

                // Increase the number of faces
                n++;
            }
        }

        /* Save the number of faces, faces id's and angles in connection */
        connection[i].n = n;
        connection[i].coeffs = (double *)malloc(n * sizeof(double));
        connection[i].faces = (int *)malloc(n * sizeof(int));

        for (k = 0; k < n; k++)
        {
            connection[i].coeffs[k] = angles[k] / sum;
            connection[i].faces[k] = facesIds[k];
        }
    }

    free(facesIds);
    free(angles);
}

void calculateFacesConnection(int nv,
                              int nf,
                              int *faces,
                              struct VerticeConnection *verticesConnection,
                              struct FacesConnection *facesConnection)
{

    /* Parameters */
    int i, j, k;
    int check;
    int index;
    int *connectedFaces;
    int n;

    /* Initialize */
    connectedFaces = (int *)malloc(50 * sizeof(int));

    for (i = 0; i < nf; i++)
    {

        n = 0;

        // First index
        index = faces[3 * i];

        for (j = 0; j < verticesConnection[index].n; j++)
        {
            if (verticesConnection[index].faces[j] != i)
            {
                connectedFaces[n] = verticesConnection[index].faces[j];
                n = n + 1;
            }
        }

        // Second index
        index = faces[3 * i + 1];

        for (j = 0; j < verticesConnection[index].n; j++)
        {

            check = 0;

            for (k = 0; k < n; k++)
            {
                if (connectedFaces[k] == verticesConnection[index].faces[j])
                {
                    check = 1;
                    break;
                }
            }

            if ((verticesConnection[index].faces[j] != i) && (check == 0))
            {
                connectedFaces[n] = verticesConnection[index].faces[j];
                n = n + 1;
            }
        }

        // Second index
        index = faces[3 * i + 2];

        for (j = 0; j < verticesConnection[index].n; j++)
        {

            check = 0;

            for (k = 0; k < n; k++)
            {
                if (connectedFaces[k] == verticesConnection[index].faces[j])
                {
                    check = 1;
                    break;
                }
            }

            if ((verticesConnection[index].faces[j] != i) && (check == 0))
            {
                connectedFaces[n] = verticesConnection[index].faces[j];
                n = n + 1;
            }
        }

        facesConnection[i].n = n;
        facesConnection[i].faces = (int *)malloc(n * sizeof(int));

        for (j = 0; j < n; j++)
        {
            facesConnection[i].faces[j] = connectedFaces[j];
        }
    }

    free(connectedFaces);
}

void calculateVerticesValues(int nv,
                             int nf,
                             struct VerticeConnection *connection,
                             double *in,
                             double *out)
{

    int i, j;

    for (i = 0; i < nv; i++)
    {
        out[i] = 0;
        for (j = 0; j < connection[i].n; j++)
        {
            out[i] = out[i] + in[connection[i].faces[j]] * connection[i].coeffs[j];
        }
    }
}

void find_exp_ratio(double S,
                    double a0,
                    double n,
                    double *r0)
{

    /* Parameters */
    int i;
    int calc;
    double step;
    double tol;
    double aux, faux;
    double dfdr, dx;
    double a, fa, b, fb;

    calc = 0;

    // Convergence
    tol = 1e-8;

    // Check convergence
    if ((1 - tol < *r0) && (*r0 < 1 + tol))
        *r0 = 1 + tol;

    aux = a0 * (1 - pow(*r0, n)) / (1 - *r0) - S;

    if ((-tol < aux) && (aux < tol))
        calc = 1;

    if (calc == 0)
    {

        // Find edges
        step = 1e-2;

        if (aux < 0)
        {
            fa = aux;
            a = *r0;
            while (aux <= 0)
            {
                *r0 = *r0 + step;
                if ((1 - tol < *r0) && (*r0 < 1 + tol))
                    *r0 = 1 + tol;
                aux = a0 * (1 - pow(*r0, n)) / (1 - *r0) - S;
                if (aux < 0)
                {
                    fa = aux;
                    a = *r0;
                }
            }
            b = *r0;
            fb = aux;
        }
        else
        {
            fb = aux;
            b = *r0;
            while (aux >= 0)
            {
                *r0 = *r0 - step;
                if ((1 - tol < *r0) && (*r0 < 1 + tol))
                    *r0 = 1 + tol;
                aux = a0 * (1 - pow(*r0, n)) / (1 - *r0) - S;
                if (aux > 0)
                {
                    fb = aux;
                    b = *r0;
                }
            }
            a = *r0;
            fa = aux;
        }

        // Find root
        for (i = 0; i < 500; i++)
        {

            aux = 0.5 * (a + b);
            if ((1 - tol < aux) && (aux < 1 + tol))
                aux = 1 + tol;
            faux = a0 * (1 - pow(aux, n)) / (1 - aux) - S;

            *r0 = aux;

            if ((-tol < faux) && (faux < tol))
            {
                break;
            }

            if (faux < 0)
            {
                a = aux;
                fa = faux;
            }
            else
            {
                b = aux;
                fb = faux;
            }
        }
    }
}

struct Point gradient(double centerValue,
                      struct Point p1,
                      struct Point p2,
                      struct Point p3,
                      struct Point e1,
                      struct Point e2,
                      struct Point e3,
                      struct Point vel,
                      double transpiration)
{

    struct Point p0;

    struct Point p01;
    struct Point p02;
    struct Point p03;

    struct Point n1;
    struct Point n2;
    struct Point n3;
    struct Point n;

    struct Point grad;

    // Gradient in plane system
    p0.x = (p1.x + p2.x + p3.x) / 3;
    p0.y = (p1.y + p2.y + p3.y) / 3;
    p0.z = centerValue;

    p01.x = p1.x - p0.x;
    p01.y = p1.y - p0.y;
    p01.z = p1.z - p0.z;

    p02.x = p2.x - p0.x;
    p02.y = p2.y - p0.y;
    p02.z = p2.z - p0.z;

    p03.x = p3.x - p0.x;
    p03.y = p3.y - p0.y;
    p03.z = p3.z - p0.z;

    n1.x = p01.y * p02.z - p01.z * p02.y;
    n1.y = p01.z * p02.x - p01.x * p02.z;
    n1.z = p01.x * p02.y - p01.y * p02.x;

    n2.x = p02.y * p03.z - p02.z * p03.y;
    n2.y = p02.z * p03.x - p02.x * p03.z;
    n2.z = p02.x * p03.y - p02.y * p03.x;

    n3.x = p03.y * p01.z - p03.z * p01.y;
    n3.y = p03.z * p01.x - p03.x * p01.z;
    n3.z = p03.x * p01.y - p03.y * p01.x;

    n.x = n1.x + n2.x + n3.x;
    n.y = n1.y + n2.y + n3.y;
    n.z = n1.z + n2.z + n3.z;

    grad.x = -n.x / n.z;
    grad.y = -n.y / n.z;
    grad.z = 0.0;

    // Convert to streamline system
    struct Point dir1;
    struct Point dir2;
    double norm;
    double s1;
    double s2;

    norm = sqrt(pow(vel.x - transpiration * e3.x, 2) + pow(vel.y - transpiration * e3.y, 2) + pow(vel.z - transpiration * e3.z, 2));

    dir1.x = (vel.x - transpiration * e3.x) / norm;
    dir1.y = (vel.y - transpiration * e3.y) / norm;
    dir1.z = (vel.z - transpiration * e3.z) / norm;

    dir2.x = e3.y * dir1.z - e3.z * dir1.y;
    dir2.y = e3.z * dir1.x - e3.x * dir1.z;
    dir2.z = e3.x * dir1.y - e3.y * dir1.x;

    s1 = grad.x * dir1.x + grad.y * dir1.y + grad.z * dir1.z;
    s2 = grad.x * dir2.x + grad.y * dir2.y + grad.z * dir2.z;

    // Output
    struct Point out;

    out.x = s1;
    out.y = s2;
    out.z = 0.0;

    return out;
}

double divergence(struct Point p1,
                  struct Point p2,
                  struct Point p3,
                  struct Point v1,
                  struct Point v2,
                  struct Point v3,
                  double area)
{

    // Sides
    double norm1;
    double norm2;
    double norm3;

    struct Point line1;
    struct Point line2;
    struct Point line3;

    struct Point vec1;
    struct Point vec2;
    struct Point vec3;

    norm1 = sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
    norm2 = sqrt(pow(p3.x - p2.x, 2) + pow(p3.y - p2.y, 2));
    norm3 = sqrt(pow(p1.x - p3.x, 2) + pow(p1.y - p3.y, 2));

    line1.x = (p2.x - p1.x) / norm1;
    line1.y = (p2.y - p1.y) / norm1;
    line1.z = 0.0;
    line2.x = (p3.x - p2.x) / norm2;
    line2.y = (p3.y - p2.y) / norm2;
    line2.z = 0.0;
    line3.x = (p1.x - p3.x) / norm3;
    line3.y = (p1.y - p3.y) / norm3;
    line3.z = 0.0;

    vec1.x = line1.y;
    vec1.y = -line1.x;
    vec1.z = 0.0;

    vec2.x = line2.y;
    vec2.y = -line2.x;
    vec2.z = 0.0;

    vec3.x = line3.y;
    vec3.y = -line3.x;
    vec3.z = 0.0;

    // Flux
    double a, b;
    double integral;

    // Line 1
    a = vec1.x * v1.x + vec1.y * v1.y;
    b = vec1.x * v2.x + vec1.y * v2.y;
    integral = 0.5 * norm1 * (a + b);

    // Line 2
    a = vec2.x * v2.x + vec2.y * v2.y;
    b = vec2.x * v3.x + vec2.y * v3.y;
    integral = integral + 0.5 * norm2 * (a + b);

    // Line 3
    a = vec3.x * v3.x + vec3.y * v3.y;
    b = vec3.x * v1.x + vec3.y * v1.y;
    integral = integral + 0.5 * norm3 * (a + b);

    return integral / area;
}

void calculateGradient(double centerValue,
                       struct Point p1,
                       struct Point p2,
                       struct Point p3,
                       struct Point e1,
                       struct Point e2,
                       struct Point e3,
                       struct Point vel,
                       double transpiration,
                       double *x, double *y)
{

    struct Point p0;

    struct Point p01;
    struct Point p02;
    struct Point p03;

    struct Point n1;
    struct Point n2;
    struct Point n3;
    struct Point n;

    struct Point grad;

    // Gradient in plane system
    p0.x = (p1.x + p2.x + p3.x) / 3;
    p0.y = (p1.y + p2.y + p3.y) / 3;
    p0.z = centerValue;

    p01.x = p1.x - p0.x;
    p01.y = p1.y - p0.y;
    p01.z = p1.z - p0.z;

    p02.x = p2.x - p0.x;
    p02.y = p2.y - p0.y;
    p02.z = p2.z - p0.z;

    p03.x = p3.x - p0.x;
    p03.y = p3.y - p0.y;
    p03.z = p3.z - p0.z;

    n1.x = p01.y * p02.z - p01.z * p02.y;
    n1.y = p01.z * p02.x - p01.x * p02.z;
    n1.z = p01.x * p02.y - p01.y * p02.x;

    n2.x = p02.y * p03.z - p02.z * p03.y;
    n2.y = p02.z * p03.x - p02.x * p03.z;
    n2.z = p02.x * p03.y - p02.y * p03.x;

    n3.x = p03.y * p01.z - p03.z * p01.y;
    n3.y = p03.z * p01.x - p03.x * p01.z;
    n3.z = p03.x * p01.y - p03.y * p01.x;

    n.x = n1.x + n2.x + n3.x;
    n.y = n1.y + n2.y + n3.y;
    n.z = n1.z + n2.z + n3.z;

    grad.x = -n.x / n.z;
    grad.y = -n.y / n.z;
    grad.z = 0.0;

    // Convert to streamline system
    struct Point dir1;
    struct Point dir2;
    double norm;
    double s1;
    double s2;

    norm = sqrt(pow(vel.x - transpiration * e3.x, 2) + pow(vel.y - transpiration * e3.y, 2) + pow(vel.z - transpiration * e3.z, 2));

    dir1.x = (vel.x - transpiration * e3.x) / norm;
    dir1.y = (vel.y - transpiration * e3.y) / norm;
    dir1.z = (vel.z - transpiration * e3.z) / norm;

    dir2.x = e3.y * dir1.z - e3.z * dir1.y;
    dir2.y = e3.z * dir1.x - e3.x * dir1.z;
    dir2.z = e3.x * dir1.y - e3.y * dir1.x;

    s1 = grad.x * dir1.x + grad.y * dir1.y + grad.z * dir1.z;
    s2 = grad.x * dir2.x + grad.y * dir2.y + grad.z * dir2.z;

    // Output
    *x = s1;
    *x = s2;
}

void calculateDivergence(struct Point p1,
                         struct Point p2,
                         struct Point p3,
                         struct Point v1,
                         struct Point v2,
                         struct Point v3,
                         double area,
                         double *out) {

    // Sides
    double norm1;
    double norm2;
    double norm3;

    struct Point line1;
    struct Point line2;
    struct Point line3;

    struct Point vec1;
    struct Point vec2;
    struct Point vec3;

    norm1 = sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
    norm2 = sqrt(pow(p3.x - p2.x, 2) + pow(p3.y - p2.y, 2));
    norm3 = sqrt(pow(p1.x - p3.x, 2) + pow(p1.y - p3.y, 2));

    line1.x = (p2.x - p1.x) / norm1;
    line1.y = (p2.y - p1.y) / norm1;
    line1.z = 0.0;
    line2.x = (p3.x - p2.x) / norm2;
    line2.y = (p3.y - p2.y) / norm2;
    line2.z = 0.0;
    line3.x = (p1.x - p3.x) / norm3;
    line3.y = (p1.y - p3.y) / norm3;
    line3.z = 0.0;

    vec1.x = line1.y;
    vec1.y = -line1.x;
    vec1.z = 0.0;

    vec2.x = line2.y;
    vec2.y = -line2.x;
    vec2.z = 0.0;

    vec3.x = line3.y;
    vec3.y = -line3.x;
    vec3.z = 0.0;

    // Flux
    double a, b;
    double integral;

    // Line 1
    a = vec1.x * v1.x + vec1.y * v1.y;
    b = vec1.x * v2.x + vec1.y * v2.y;
    integral = 0.5 * norm1 * (a + b);

    // Line 2
    a = vec2.x * v2.x + vec2.y * v2.y;
    b = vec2.x * v3.x + vec2.y * v3.y;
    integral = integral + 0.5 * norm2 * (a + b);

    // Line 3
    a = vec3.x * v3.x + vec3.y * v3.y;
    b = vec3.x * v1.x + vec3.y * v1.y;
    integral = integral + 0.5 * norm3 * (a + b);

    *out = integral / area;
}

/*
#####################################################
    LINEAR SYSTEM
#####################################################
*/
void solveLinearSystem(lapack_int n,
                       double *A,
                       double *b)
{

    /* Locals */
    lapack_int info;

    /* Local arrays */
    lapack_int *ipiv;

    /* Initialization */
    ipiv = (lapack_int *)malloc(n * sizeof(lapack_int));

    /* Solve the equations A*X = B */
    info = LAPACKE_dgesv(LAPACK_ROW_MAJOR, n, 1, A, n, ipiv, b, 1);
}

/*
#####################################################
    POTENTIAL FLOW
#####################################################
*/
void sourceFunc(struct Point p,
                struct Point p1,
                struct Point p2,
                struct Point p3,
                struct Point e1,
                struct Point e2,
                struct Point e3,
                double area,
                double maxDistance,
                double *vel)
{

    double u, v, w;
    double distance = norm(p);

    if (distance > maxDistance)
    {

        double pNorm3 = pow(norm(p), 3);

        u = FACTOR * area * p.x / pNorm3;
        v = FACTOR * area * p.y / pNorm3;
        w = FACTOR * area * p.z / pNorm3;
    }
    else
    {

        // Velocity parameters
        double r1, r2, r3;
        double l1, l2, l3;
        double h1, h2, h3;

        double d12, d23, d31;
        double m12, m23, m31;
        double ln12, ln23, ln31;

        // Calculate
        r1 = sqrt(pow(p.x - p1.x, 2) + pow(p.y - p1.y, 2) + pow(p.z, 2));
        r2 = sqrt(pow(p.x - p2.x, 2) + pow(p.y - p2.y, 2) + pow(p.z, 2));
        r3 = sqrt(pow(p.x - p3.x, 2) + pow(p.y - p3.y, 2) + pow(p.z, 2));

        l1 = pow(p.x - p1.x, 2) + pow(p.z, 2);
        l2 = pow(p.x - p2.x, 2) + pow(p.z, 2);
        l3 = pow(p.x - p3.x, 2) + pow(p.z, 2);

        h1 = (p.x - p1.x) * (p.y - p1.y);
        h2 = (p.x - p2.x) * (p.y - p2.y);
        h3 = (p.x - p3.x) * (p.y - p3.y);

        d12 = sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
        m12 = division(p2.y - p1.y, p2.x - p1.x);

        d23 = sqrt(pow(p3.x - p2.x, 2) + pow(p3.y - p2.y, 2));
        m23 = division(p3.y - p2.y, p3.x - p2.x);

        d31 = sqrt(pow(p1.x - p3.x, 2) + pow(p1.y - p3.y, 2));
        m31 = division(p1.y - p3.y, p1.x - p3.x);

        ln12 = log(division(r1 + r2 - d12, r1 + r2 + d12));
        ln23 = log(division(r2 + r3 - d23, r2 + r3 + d23));
        ln31 = log(division(r3 + r1 - d31, r3 + r1 + d31));

        u = -FACTOR * (division((p2.y - p1.y), d12) * ln12 + division((p3.y - p2.y), d23) * ln23 + division((p1.y - p3.y), d31) * ln31);
        v = FACTOR * (division((p2.x - p1.x), d12) * ln12 + division((p3.x - p2.x), d23) * ln23 + division((p1.x - p3.x), d31) * ln31);
        w = -FACTOR * (atan(division(m12 * l1 - h1, p.z * r1)) - atan(division(m12 * l2 - h2, p.z * r2)) + atan(division(m23 * l2 - h2, p.z * r2)) - atan(division(m23 * l3 - h3, p.z * r3)) + atan(division(m31 * l3 - h3, p.z * r3)) - atan(division(m31 * l1 - h1, p.z * r1)));
    }

    vel[0] = u * e1.x + v * e2.x + w * e3.x;
    vel[1] = u * e1.y + v * e2.y + w * e3.y;
    vel[2] = u * e1.z + v * e2.z + w * e3.z;
}

void lineFunc(struct Point p,
              struct Point p1,
              struct Point p2,
              double *vel)
{

    struct Point r1 = {p1.x - p.x, p1.y - p.y, p1.z - p.z};
    struct Point r2 = {p2.x - p.x, p2.y - p.y, p2.z - p.z};

    struct Point r1xr2 = cross(r1, r2);

    double r1Norm = norm(r1);
    double r2Norm = norm(r2);

    double r1xr2Norm2 = pow(norm(r1xr2), 2);

    double dot = (1 / r1xr2Norm2) * ((r1.x - r2.x) * (r1.x / r1Norm - r2.x / r2Norm) + (r1.y - r2.y) * (r1.y / r1Norm - r2.y / r2Norm) + (r1.z - r2.z) * (r1.z / r1Norm - r2.z / r2Norm));

    vel[0] = FACTOR * r1xr2.x * dot;
    vel[1] = FACTOR * r1xr2.y * dot;
    vel[2] = FACTOR * r1xr2.z * dot;
}

void doubletFunc(struct Point p,
                 struct Point p1,
                 struct Point p2,
                 struct Point p3,
                 struct Point e1,
                 struct Point e2,
                 struct Point e3,
                 double area,
                 double maxDistance,
                 double *vel)
{

    double distance = norm(p);

    if (distance > maxDistance)
    {

        double pxLocal = p.x * e1.x + p.y * e1.y + p.z * e1.z;
        double pyLocal = p.x * e2.x + p.y * e2.y + p.z * e2.z;
        double pzLocal = p.x * e3.x + p.y * e3.y + p.z * e3.z;
        double den = pow(pxLocal * pxLocal + pyLocal * pyLocal + pzLocal * pzLocal, 2.5);

        double u = 0.75 * FACTOR * area * pzLocal * pxLocal / den;
        double v = 0.75 * FACTOR * area * pzLocal * pyLocal / den;
        double w = -FACTOR * area * (pxLocal * pxLocal + pyLocal * pyLocal - 2 * pzLocal * pzLocal) / den;

        vel[0] = u * e1.x + v * e2.x + w * e3.x;
        vel[1] = u * e1.y + v * e2.y + w * e3.y;
        vel[2] = u * e1.z + v * e2.z + w * e3.z;
    }
    else
    {

        double u, v, w;

        double *vel1 = (double *)malloc(3 * sizeof(double *));
        double *vel2 = (double *)malloc(3 * sizeof(double *));
        double *vel3 = (double *)malloc(3 * sizeof(double *));

        lineFunc(p, p1, p2, vel1);
        lineFunc(p, p2, p3, vel2);
        lineFunc(p, p3, p1, vel3);

        u = vel1[0] + vel2[0] + vel3[0];
        v = vel1[1] + vel2[1] + vel3[1];
        w = vel1[2] + vel2[2] + vel3[2];

        vel[0] = u * e1.x + v * e2.x + w * e3.x;
        vel[1] = u * e1.y + v * e2.y + w * e3.y;
        vel[2] = u * e1.z + v * e2.z + w * e3.z;

        free(vel1);
        free(vel2);
        free(vel3);
    }
}

void createLinearSystem(int n,
                        double *facesAreas,
                        double *facesMaxDistance,
                        double *facesCenter,
                        double *controlPoints,
                        double *p1,
                        double *p2,
                        double *p3,
                        double *e1,
                        double *e2,
                        double *e3,
                        double *freestream,
                        double *sigma,
                        int nSpanLeftWing,
                        int nWakeLeftWing,
                        int *leftWingGrid,
                        double *leftWingVertices,
                        int *leftWingFaces,
                        int nSpanRightWing,
                        int nWakeRightWing,
                        int *rightWingGrid,
                        double *rightWingVertices,
                        int *rightWingFaces,
                        int nSpanTail,
                        int nWakeTail,
                        int *tailGrid,
                        double *tailVertices,
                        int *tailFaces,
                        double *matrix,
                        double *array,
                        double *matrixVelx,
                        double *matrixVely,
                        double *matrixVelz,
                        double *arrayVel)
{

    ///---------------------------------------///
    /// Parameters

    // Loops
    int i, j, k, l;

    // Point
    int i3D1, i3D2, i3D3, j3D1, j3D2, j3D3;
    int j2D1, j2D2;
    int indexLine1, indexLine2;
    struct Point p;
    struct Point p1Line;
    struct Point p2Line;
    struct Point pLocal;
    struct Point p1Local;
    p1Local.z = 0.0;
    struct Point p2Local;
    p2Local.z = 0.0;
    struct Point p3Local;
    p3Local.z = 0.0;

    // Base vectors
    struct Point e3iPoint;

    struct Point e1jPoint;
    struct Point e2jPoint;
    struct Point e3jPoint;

    // Velocities
    double *sourceVel = (double *)malloc(3 * sizeof(double));
    double *doubletVel = (double *)malloc(3 * sizeof(double));
    double *lineVel = (double *)malloc(3 * sizeof(double));

    ///---------------------------------------///
    /// Linear system

    // Create
    for (i = 0; i < n; i++)
    {

        i3D1 = i * 3;
        i3D2 = i3D1 + 1;
        i3D3 = i3D1 + 2;

        e3iPoint.x = e3[i3D1];
        e3iPoint.y = e3[i3D2];
        e3iPoint.z = e3[i3D3];

        array[i] = 0;
        arrayVel[i * 3] = 0;
        arrayVel[i * 3 + 1] = 0;
        arrayVel[i * 3 + 2] = 0;

        // Surface
        // Effect of j on i
        for (j = 0; j < n; j++)
        {

            j3D1 = j * 3;
            j3D2 = j3D1 + 1;
            j3D3 = j3D1 + 2;

            j2D1 = j * 2;
            j2D2 = j2D1 + 1;

            // Points
            e1jPoint.x = e1[j3D1];
            e1jPoint.y = e1[j3D2];
            e1jPoint.z = e1[j3D3];
            e2jPoint.x = e2[j3D1];
            e2jPoint.y = e2[j3D2];
            e2jPoint.z = e2[j3D3];
            e3jPoint.x = e3[j3D1];
            e3jPoint.y = e3[j3D2];
            e3jPoint.z = e3[j3D3];

            p.x = controlPoints[i3D1] - facesCenter[j3D1];
            p.y = controlPoints[i3D2] - facesCenter[j3D2];
            p.z = controlPoints[i3D3] - facesCenter[j3D3];

            pLocal.x = p.x * e1jPoint.x + p.y * e1jPoint.y + p.z * e1jPoint.z;
            pLocal.y = p.x * e2jPoint.x + p.y * e2jPoint.y + p.z * e2jPoint.z;
            pLocal.z = p.x * e3jPoint.x + p.y * e3jPoint.y + p.z * e3jPoint.z;

            p1Local.x = p1[j2D1];
            p1Local.y = p1[j2D2];
            p2Local.x = p2[j2D1];
            p2Local.y = p2[j2D2];
            p3Local.x = p3[j2D1];
            p3Local.y = p3[j2D2];

            sourceFunc(pLocal, p1Local, p2Local, p3Local, e1jPoint, e2jPoint, e3jPoint, facesAreas[j], facesMaxDistance[j], sourceVel);
            doubletFunc(pLocal, p1Local, p2Local, p3Local, e1jPoint, e2jPoint, e3jPoint, facesAreas[j], facesMaxDistance[j], doubletVel);

            matrix[i * n + j] = doubletVel[0] * e3iPoint.x + doubletVel[1] * e3iPoint.y + doubletVel[2] * e3iPoint.z;
            array[i] = array[i] - sigma[j] * (sourceVel[0] * e3iPoint.x + sourceVel[1] * e3iPoint.y + sourceVel[2] * e3iPoint.z);

            matrixVelx[i * n + j] = doubletVel[0];
            matrixVely[i * n + j] = doubletVel[1];
            matrixVelz[i * n + j] = doubletVel[2];

            arrayVel[i * 3] = arrayVel[i * 3] + sigma[j] * sourceVel[0];
            arrayVel[i * 3 + 1] = arrayVel[i * 3 + 1] + sigma[j] * sourceVel[1];
            arrayVel[i * 3 + 2] = arrayVel[i * 3 + 2] + sigma[j] * sourceVel[2];
        }

        array[i] = array[i] - (freestream[0] * e3iPoint.x + freestream[1] * e3iPoint.y + freestream[2] * e3iPoint.z);

        arrayVel[i * 3] = arrayVel[i * 3] + freestream[0];
        arrayVel[i * 3 + 1] = arrayVel[i * 3 + 1] + freestream[1];
        arrayVel[i * 3 + 2] = arrayVel[i * 3 + 2] + freestream[2];

        // Wake
        p.x = controlPoints[i3D1];
        p.y = controlPoints[i3D2];
        p.z = controlPoints[i3D3];

        // Left wing
        for (k = 0; k < nSpanLeftWing; k++)
        {

            for (l = 0; l < nWakeLeftWing - 1; l++)
            {

                indexLine1 = leftWingGrid[k * nWakeLeftWing + l] * 3;
                p1Line.x = leftWingVertices[indexLine1];
                p1Line.y = leftWingVertices[indexLine1 + 1];
                p1Line.z = leftWingVertices[indexLine1 + 2];

                indexLine2 = leftWingGrid[k * nWakeLeftWing + l + 1] * 3;
                p2Line.x = leftWingVertices[indexLine2];
                p2Line.y = leftWingVertices[indexLine2 + 1];
                p2Line.z = leftWingVertices[indexLine2 + 2];

                lineFunc(p, p1Line, p2Line, lineVel);

                if (k == 0)
                {

                    matrix[i * n + leftWingFaces[k * 2]] = matrix[i * n + leftWingFaces[k * 2]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + leftWingFaces[k * 2 + 1]] = matrix[i * n + leftWingFaces[k * 2 + 1]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + leftWingFaces[k * 2]] = matrixVelx[i * n + leftWingFaces[k * 2]] - lineVel[0];
                    matrixVely[i * n + leftWingFaces[k * 2]] = matrixVely[i * n + leftWingFaces[k * 2]] - lineVel[1];
                    matrixVelz[i * n + leftWingFaces[k * 2]] = matrixVelz[i * n + leftWingFaces[k * 2]] - lineVel[2];

                    matrixVelx[i * n + leftWingFaces[k * 2 + 1]] = matrixVelx[i * n + leftWingFaces[k * 2 + 1]] + lineVel[0];
                    matrixVely[i * n + leftWingFaces[k * 2 + 1]] = matrixVely[i * n + leftWingFaces[k * 2 + 1]] + lineVel[1];
                    matrixVelz[i * n + leftWingFaces[k * 2 + 1]] = matrixVelz[i * n + leftWingFaces[k * 2 + 1]] + lineVel[2];
                }
                else if (k == nSpanLeftWing - 1)
                {

                    matrix[i * n + leftWingFaces[(k - 1) * 2]] = matrix[i * n + leftWingFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrix[i * n + leftWingFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + leftWingFaces[(k - 1) * 2]] = matrixVelx[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + leftWingFaces[(k - 1) * 2]] = matrixVely[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + leftWingFaces[(k - 1) * 2]] = matrixVelz[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[2];
                }
                else
                {

                    matrix[i * n + leftWingFaces[(k - 1) * 2]] = matrix[i * n + leftWingFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrix[i * n + leftWingFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + leftWingFaces[(k - 1) * 2]] = matrixVelx[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + leftWingFaces[(k - 1) * 2]] = matrixVely[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + leftWingFaces[(k - 1) * 2]] = matrixVelz[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[2];

                    matrix[i * n + leftWingFaces[k * 2]] = matrix[i * n + leftWingFaces[k * 2]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + leftWingFaces[k * 2 + 1]] = matrix[i * n + leftWingFaces[k * 2 + 1]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + leftWingFaces[k * 2]] = matrixVelx[i * n + leftWingFaces[k * 2]] - lineVel[0];
                    matrixVely[i * n + leftWingFaces[k * 2]] = matrixVely[i * n + leftWingFaces[k * 2]] - lineVel[1];
                    matrixVelz[i * n + leftWingFaces[k * 2]] = matrixVelz[i * n + leftWingFaces[k * 2]] - lineVel[2];

                    matrixVelx[i * n + leftWingFaces[k * 2 + 1]] = matrixVelx[i * n + leftWingFaces[k * 2 + 1]] + lineVel[0];
                    matrixVely[i * n + leftWingFaces[k * 2 + 1]] = matrixVely[i * n + leftWingFaces[k * 2 + 1]] + lineVel[1];
                    matrixVelz[i * n + leftWingFaces[k * 2 + 1]] = matrixVelz[i * n + leftWingFaces[k * 2 + 1]] + lineVel[2];

                    indexLine1 = leftWingGrid[(k - 1) * nWakeLeftWing] * 3;
                    p1Line.x = leftWingVertices[indexLine1];
                    p1Line.y = leftWingVertices[indexLine1 + 1];
                    p1Line.z = leftWingVertices[indexLine1 + 2];

                    indexLine2 = leftWingGrid[k * nWakeLeftWing] * 3;
                    p2Line.x = leftWingVertices[indexLine2];
                    p2Line.y = leftWingVertices[indexLine2 + 1];
                    p2Line.z = leftWingVertices[indexLine2 + 2];

                    lineFunc(p, p1Line, p2Line, lineVel);

                    matrix[i * n + leftWingFaces[(k - 1) * 2]] = matrix[i * n + leftWingFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrix[i * n + leftWingFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + leftWingFaces[(k - 1) * 2]] = matrixVelx[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + leftWingFaces[(k - 1) * 2]] = matrixVely[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + leftWingFaces[(k - 1) * 2]] = matrixVelz[i * n + leftWingFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + leftWingFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + leftWingFaces[(k - 1) * 2 + 1]] - lineVel[2];
                }
            }
        }

        // Right wing
        for (k = 0; k < nSpanRightWing; k++)
        {

            for (l = 0; l < nWakeRightWing - 1; l++)
            {

                indexLine1 = rightWingGrid[k * nWakeRightWing + l] * 3;
                p1Line.x = rightWingVertices[indexLine1];
                p1Line.y = rightWingVertices[indexLine1 + 1];
                p1Line.z = rightWingVertices[indexLine1 + 2];

                indexLine2 = rightWingGrid[k * nWakeRightWing + l + 1] * 3;
                p2Line.x = rightWingVertices[indexLine2];
                p2Line.y = rightWingVertices[indexLine2 + 1];
                p2Line.z = rightWingVertices[indexLine2 + 2];

                lineFunc(p, p1Line, p2Line, lineVel);

                if (k == 0)
                {

                    matrix[i * n + rightWingFaces[k * 2]] = matrix[i * n + rightWingFaces[k * 2]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + rightWingFaces[k * 2 + 1]] = matrix[i * n + rightWingFaces[k * 2 + 1]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + rightWingFaces[k * 2]] = matrixVelx[i * n + rightWingFaces[k * 2]] - lineVel[0];
                    matrixVely[i * n + rightWingFaces[k * 2]] = matrixVely[i * n + rightWingFaces[k * 2]] - lineVel[1];
                    matrixVelz[i * n + rightWingFaces[k * 2]] = matrixVelz[i * n + rightWingFaces[k * 2]] - lineVel[2];

                    matrixVelx[i * n + rightWingFaces[k * 2 + 1]] = matrixVelx[i * n + rightWingFaces[k * 2 + 1]] + lineVel[0];
                    matrixVely[i * n + rightWingFaces[k * 2 + 1]] = matrixVely[i * n + rightWingFaces[k * 2 + 1]] + lineVel[1];
                    matrixVelz[i * n + rightWingFaces[k * 2 + 1]] = matrixVelz[i * n + rightWingFaces[k * 2 + 1]] + lineVel[2];
                }
                else if (k == nSpanRightWing - 1)
                {

                    matrix[i * n + rightWingFaces[(k - 1) * 2]] = matrix[i * n + rightWingFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrix[i * n + rightWingFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + rightWingFaces[(k - 1) * 2]] = matrixVelx[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + rightWingFaces[(k - 1) * 2]] = matrixVely[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + rightWingFaces[(k - 1) * 2]] = matrixVelz[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[2];
                }
                else
                {

                    matrix[i * n + rightWingFaces[(k - 1) * 2]] = matrix[i * n + rightWingFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrix[i * n + rightWingFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + rightWingFaces[(k - 1) * 2]] = matrixVelx[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + rightWingFaces[(k - 1) * 2]] = matrixVely[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + rightWingFaces[(k - 1) * 2]] = matrixVelz[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[2];

                    matrix[i * n + rightWingFaces[k * 2]] = matrix[i * n + rightWingFaces[k * 2]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + rightWingFaces[k * 2 + 1]] = matrix[i * n + rightWingFaces[k * 2 + 1]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + rightWingFaces[k * 2]] = matrixVelx[i * n + rightWingFaces[k * 2]] - lineVel[0];
                    matrixVely[i * n + rightWingFaces[k * 2]] = matrixVely[i * n + rightWingFaces[k * 2]] - lineVel[1];
                    matrixVelz[i * n + rightWingFaces[k * 2]] = matrixVelz[i * n + rightWingFaces[k * 2]] - lineVel[2];

                    matrixVelx[i * n + rightWingFaces[k * 2 + 1]] = matrixVelx[i * n + rightWingFaces[k * 2 + 1]] + lineVel[0];
                    matrixVely[i * n + rightWingFaces[k * 2 + 1]] = matrixVely[i * n + rightWingFaces[k * 2 + 1]] + lineVel[1];
                    matrixVelz[i * n + rightWingFaces[k * 2 + 1]] = matrixVelz[i * n + rightWingFaces[k * 2 + 1]] + lineVel[2];

                    indexLine1 = rightWingGrid[(k - 1) * nWakeRightWing] * 3;
                    p1Line.x = rightWingVertices[indexLine1];
                    p1Line.y = rightWingVertices[indexLine1 + 1];
                    p1Line.z = rightWingVertices[indexLine1 + 2];

                    indexLine2 = rightWingGrid[k * nWakeRightWing] * 3;
                    p2Line.x = rightWingVertices[indexLine2];
                    p2Line.y = rightWingVertices[indexLine2 + 1];
                    p2Line.z = rightWingVertices[indexLine2 + 2];

                    lineFunc(p, p1Line, p2Line, lineVel);

                    matrix[i * n + rightWingFaces[(k - 1) * 2]] = matrix[i * n + rightWingFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrix[i * n + rightWingFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + rightWingFaces[(k - 1) * 2]] = matrixVelx[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + rightWingFaces[(k - 1) * 2]] = matrixVely[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + rightWingFaces[(k - 1) * 2]] = matrixVelz[i * n + rightWingFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + rightWingFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + rightWingFaces[(k - 1) * 2 + 1]] - lineVel[2];
                }
            }
        }

        // Tail
        for (k = 0; k < nSpanTail; k++)
        {

            for (l = 0; l < nWakeTail - 1; l++)
            {

                indexLine1 = tailGrid[k * nWakeTail + l] * 3;
                p1Line.x = tailVertices[indexLine1];
                p1Line.y = tailVertices[indexLine1 + 1];
                p1Line.z = tailVertices[indexLine1 + 2];

                indexLine2 = tailGrid[k * nWakeTail + l + 1] * 3;
                p2Line.x = tailVertices[indexLine2];
                p2Line.y = tailVertices[indexLine2 + 1];
                p2Line.z = tailVertices[indexLine2 + 2];

                lineFunc(p, p1Line, p2Line, lineVel);

                if (k == 0)
                {

                    matrix[i * n + tailFaces[k * 2]] = matrix[i * n + tailFaces[k * 2]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + tailFaces[k * 2 + 1]] = matrix[i * n + tailFaces[k * 2 + 1]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + tailFaces[k * 2]] = matrixVelx[i * n + tailFaces[k * 2]] - lineVel[0];
                    matrixVely[i * n + tailFaces[k * 2]] = matrixVely[i * n + tailFaces[k * 2]] - lineVel[1];
                    matrixVelz[i * n + tailFaces[k * 2]] = matrixVelz[i * n + tailFaces[k * 2]] - lineVel[2];

                    matrixVelx[i * n + tailFaces[k * 2 + 1]] = matrixVelx[i * n + tailFaces[k * 2 + 1]] + lineVel[0];
                    matrixVely[i * n + tailFaces[k * 2 + 1]] = matrixVely[i * n + tailFaces[k * 2 + 1]] + lineVel[1];
                    matrixVelz[i * n + tailFaces[k * 2 + 1]] = matrixVelz[i * n + tailFaces[k * 2 + 1]] + lineVel[2];
                }
                else if (k == nSpanTail - 1)
                {

                    matrix[i * n + tailFaces[(k - 1) * 2]] = matrix[i * n + tailFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + tailFaces[(k - 1) * 2 + 1]] = matrix[i * n + tailFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + tailFaces[(k - 1) * 2]] = matrixVelx[i * n + tailFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + tailFaces[(k - 1) * 2]] = matrixVely[i * n + tailFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + tailFaces[(k - 1) * 2]] = matrixVelz[i * n + tailFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[2];
                }
                else
                {

                    matrix[i * n + tailFaces[(k - 1) * 2]] = matrix[i * n + tailFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + tailFaces[(k - 1) * 2 + 1]] = matrix[i * n + tailFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + tailFaces[(k - 1) * 2]] = matrixVelx[i * n + tailFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + tailFaces[(k - 1) * 2]] = matrixVely[i * n + tailFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + tailFaces[(k - 1) * 2]] = matrixVelz[i * n + tailFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[2];

                    matrix[i * n + tailFaces[k * 2]] = matrix[i * n + tailFaces[k * 2]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + tailFaces[k * 2 + 1]] = matrix[i * n + tailFaces[k * 2 + 1]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + tailFaces[k * 2]] = matrixVelx[i * n + tailFaces[k * 2]] - lineVel[0];
                    matrixVely[i * n + tailFaces[k * 2]] = matrixVely[i * n + tailFaces[k * 2]] - lineVel[1];
                    matrixVelz[i * n + tailFaces[k * 2]] = matrixVelz[i * n + tailFaces[k * 2]] - lineVel[2];

                    matrixVelx[i * n + tailFaces[k * 2 + 1]] = matrixVelx[i * n + tailFaces[k * 2 + 1]] + lineVel[0];
                    matrixVely[i * n + tailFaces[k * 2 + 1]] = matrixVely[i * n + tailFaces[k * 2 + 1]] + lineVel[1];
                    matrixVelz[i * n + tailFaces[k * 2 + 1]] = matrixVelz[i * n + tailFaces[k * 2 + 1]] + lineVel[2];

                    indexLine1 = tailGrid[(k - 1) * nWakeTail] * 3;
                    p1Line.x = tailVertices[indexLine1];
                    p1Line.y = tailVertices[indexLine1 + 1];
                    p1Line.z = tailVertices[indexLine1 + 2];

                    indexLine2 = tailGrid[k * nWakeTail] * 3;
                    p2Line.x = tailVertices[indexLine2];
                    p2Line.y = tailVertices[indexLine2 + 1];
                    p2Line.z = tailVertices[indexLine2 + 2];

                    lineFunc(p, p1Line, p2Line, lineVel);

                    matrix[i * n + tailFaces[(k - 1) * 2]] = matrix[i * n + tailFaces[(k - 1) * 2]] + (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);
                    matrix[i * n + tailFaces[(k - 1) * 2 + 1]] = matrix[i * n + tailFaces[(k - 1) * 2 + 1]] - (e3iPoint.x * lineVel[0] + e3iPoint.y * lineVel[1] + e3iPoint.z * lineVel[2]);

                    matrixVelx[i * n + tailFaces[(k - 1) * 2]] = matrixVelx[i * n + tailFaces[(k - 1) * 2]] + lineVel[0];
                    matrixVely[i * n + tailFaces[(k - 1) * 2]] = matrixVely[i * n + tailFaces[(k - 1) * 2]] + lineVel[1];
                    matrixVelz[i * n + tailFaces[(k - 1) * 2]] = matrixVelz[i * n + tailFaces[(k - 1) * 2]] + lineVel[2];

                    matrixVelx[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVelx[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[0];
                    matrixVely[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVely[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[1];
                    matrixVelz[i * n + tailFaces[(k - 1) * 2 + 1]] = matrixVelz[i * n + tailFaces[(k - 1) * 2 + 1]] - lineVel[2];
                }
            }
        }
    }

    free(sourceVel);
    free(doubletVel);
    free(lineVel);
}

void calculateDoubletDistribution(int n,
                                  double *A,
                                  double *b,
                                  double *transpiration,
                                  double *sol)
{

    /* Loop parameter */
    int i;

    /* Copy the input */
    double *aux = (double *)malloc(n * n * sizeof(double));
    for (i = 0; i < n * n; i++)
        aux[i] = A[i];
    for (i = 0; i < n; i++)
        sol[i] = b[i] + transpiration[i];

    /* Solve the linear system */
    solveLinearSystem(n, aux, sol);

    free(aux);
}

void calculateSurfaceParameters(int n,
                                double *matrixVelx,
                                double *matrixVely,
                                double *matrixVelz,
                                double *arrayVel,
                                double *doublet,
                                double freestream,
                                double *velx, double *vely, double *velz,
                                double *velNorm,
                                double *cp,
                                double *mach,
                                double sound_speed)
{

    /* Loop parameter */
    int i, j, line1, line2, point;

    /* Calculate parameters */
    for (i = 0; i < n; i++)
    {

        line1 = i * 3;

        // Set velocity equal 0
        velx[i] = arrayVel[line1];
        vely[i] = arrayVel[line1 + 1];
        velz[i] = arrayVel[line1 + 2];

        // Current line
        line2 = i * n;

        for (j = 0; j < n; j++)
        {

            point = line2 + j;

            velx[i] = velx[i] + matrixVelx[point] * doublet[j];
            vely[i] = vely[i] + matrixVely[point] * doublet[j];
            velz[i] = velz[i] + matrixVelz[point] * doublet[j];
        }

        // Velocity norm
        velNorm[i] = sqrt(velx[i] * velx[i] + vely[i] * vely[i] + velz[i] * velz[i]);

        // Pressure coefficient
        cp[i] = 1 - pow(velNorm[i] / freestream, 2);

        // Mach
        mach[i] = velNorm[i] / sound_speed;
    }
}

/*
#####################################################
    BOUNDARY LAYER
#####################################################
*/
/******************************************************************************/

double r8vec_dot ( int n, double a1[], double a2[] )

/******************************************************************************/
/*
  Purpose:

    R8VEC_DOT computes the dot product of a pair of R8VEC's.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    26 July 2007

  Author:

    John Burkardt

  Parameters:

    Input, int N, the number of entries in the vectors.

    Input, double A1[N], A2[N], the two vectors to be considered.

    Output, double R8VEC_DOT, the dot product of the vectors.
*/
{
  int i;
  double value;

  value = 0.0;
  for ( i = 0; i < n; i++ )
  {
    value = value + a1[i] * a2[i];
  }
  return value;
}
/******************************************************************************/

double *r8vec_uniform_01 ( int n, int *seed )

/******************************************************************************/
/*
  Purpose:

    R8VEC_UNIFORM_01 fills a double precision vector with unit pseudorandom values.

  Discussion:

    This routine implements the recursion

      seed = 16807 * seed mod ( 2^31 - 1 )
      unif = seed / ( 2^31 - 1 )

    The integer arithmetic never requires more than 32 bits,
    including a sign bit.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    19 August 2004

  Author:

    John Burkardt

  Reference:

    Paul Bratley, Bennett Fox, Linus Schrage,
    A Guide to Simulation,
    Second Edition,
    Springer, 1987,
    ISBN: 0387964673,
    LC: QA76.9.C65.B73.

    Bennett Fox,
    Algorithm 647:
    Implementation and Relative Efficiency of Quasirandom
    Sequence Generators,
    ACM Transactions on Mathematical Software,
    Volume 12, Number 4, December 1986, pages 362-376.

    Pierre L'Ecuyer,
    Random Number Generation,
    in Handbook of Simulation,
    edited by Jerry Banks,
    Wiley, 1998,
    ISBN: 0471134031,
    LC: T57.62.H37.

    Peter Lewis, Allen Goodman, James Miller,
    A Pseudo-Random Number Generator for the System/360,
    IBM Systems Journal,
    Volume 8, Number 2, 1969, pages 136-143.

  Parameters:

    Input, int N, the number of entries in the vector.

    Input/output, int *SEED, a seed for the random number generator.

    Output, double R8VEC_UNIFORM_01[N], the vector of pseudorandom values.
*/
{
  int i;
  int k;
  double *r;

  r = ( double * ) malloc ( n * sizeof ( double ) );

  for ( i = 0; i < n; i++ )
  {
    k = *seed / 127773;

    *seed = 16807 * ( *seed - k * 127773 ) - k * 2836;

    if ( *seed < 0 )
    {
      *seed = *seed + 2147483647;
    }

    r[i] = ( double ) ( *seed ) * 4.656612875E-10;
  }

  return r;
}
/******************************************************************************/

void rearrange_cr ( int n, int nz_num, int ia[], int ja[], double a[] )

/******************************************************************************/
/*
  Purpose:

    REARRANGE_CR sorts a sparse compressed row matrix.

  Discussion:

    This routine guarantees that the entries in the CR matrix
    are properly sorted.

    After the sorting, the entries of the matrix are rearranged in such
    a way that the entries of each column are listed in ascending order
    of their column values.

    The matrix A is assumed to be stored in compressed row format.  Only
    the nonzero entries of A are stored.  The vector JA stores the
    column index of the nonzero value.  The nonzero values are sorted
    by row, and the compressed row vector IA then has the property that
    the entries in A and JA that correspond to row I occur in indices
    IA[I] through IA[I+1]-1.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    18 July 2007

  Author:

    Lili Ju

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[N+1], the compressed row index vector.

    Input/output, int JA[NZ_NUM], the column indices of the matrix values.
    On output, the order of the entries of JA may have changed because of
    the sorting.

    Input/output, double A[NZ_NUM], the matrix values.  On output, the
    order of the entries may have changed because of the sorting.
*/
{
  double dtemp;
  int i;
  int is;
  int itemp;
  int j;
  int j1;
  int j2;
  int k;

  for ( i = 0; i < n; i++ )
  {
    j1 = ia[i];
    j2 = ia[i+1];
    is = j2 - j1;

    for ( k = 1; k < is; k++ ) 
    {
      for ( j = j1; j < j2 - k; j++ ) 
      {
        if ( ja[j+1] < ja[j] ) 
        {
          itemp = ja[j+1];
          ja[j+1] =  ja[j];
          ja[j] =  itemp;

          dtemp = a[j+1];
          a[j+1] =  a[j];
          a[j] = dtemp;
        }
      }
    }
  }
  return;
}
/******************************************************************************/

void timestamp ( )

/******************************************************************************/
/*
  Purpose:

    TIMESTAMP prints the current YMDHMS date as a time stamp.

  Example:

    31 May 2001 09:45:54 AM

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    24 September 2003

  Author:

    John Burkardt

  Parameters:

    None
*/
{
# define TIME_SIZE 40

  static char time_buffer[TIME_SIZE];
  const struct tm *tm;
  time_t now;

  now = time ( NULL );
  tm = localtime ( &now );

  strftime ( time_buffer, TIME_SIZE, "%d %B %Y %I:%M:%S %p", tm );

  printf ( "%s\n", time_buffer );

  return;
# undef TIME_SIZE
}
/******************************************************************************/

void mult_givens ( double c, double s, int k, double *g )

/******************************************************************************/
/*
  Purpose:

    MULT_GIVENS applies a Givens rotation to two vector elements.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    10 August 2006

  Author:

    Lili Ju

  Reference:

    Richard Barrett, Michael Berry, Tony Chan, James Demmel,
    June Donato, Jack Dongarra, Victor Eijkhout, Roidan Pozo,
    Charles Romine, Henk van der Vorst,
    Templates for the Solution of Linear Systems:
    Building Blocks for Iterative Methods,
    SIAM, 1994,
    ISBN: 0898714710,
    LC: QA297.8.T45.

    Tim Kelley,
    Iterative Methods for Linear and Nonlinear Equations,
    SIAM, 2004,
    ISBN: 0898713528,
    LC: QA297.8.K45.

    Yousef Saad,
    Iterative Methods for Sparse Linear Systems,
    Second Edition,
    SIAM, 20003,
    ISBN: 0898715342,
    LC: QA188.S17.

  Parameters:

    Input, double C, S, the cosine and sine of a Givens
    rotation.

    Input, int K, indicates the location of the first vector entry.

    Input/output, double G[K+2], the vector to be modified.  On output,
    the Givens rotation has been applied to entries G(K) and G(K+1).
*/
{
  double g1;
  double g2;

  g1 = c * g[k] - s * g[k+1];
  g2 = s * g[k] + c * g[k+1];

  g[k]   = g1;
  g[k+1] = g2;

  return;
}
/******************************************************************************/

void atx_cr ( int n, int nz_num, int ia[], int ja[], double a[], double x[],
  double w[] )

/******************************************************************************/
/*
  Purpose:

    ATX_CR computes A'*x for a matrix stored in sparse compressed row form.

  Discussion:

    The matrix A is assumed to be stored in compressed row format.  Only
    the nonzero entries of A are stored.  The vector JA stores the
    column index of the nonzero value.  The nonzero values are sorted
    by row, and the compressed row vector IA then has the property that
    the entries in A and JA that correspond to row I occur in indices
    IA[I] through IA[I+1]-1.

    For this version of MGMRES, the row and column indices are assumed
    to use the C/C++ convention, in which indexing begins at 0.

    If your index vectors IA and JA are set up so that indexing is based 
    at 1, each use of those vectors should be shifted down by 1.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    17 July 2007

  Author:

    Lili Ju

  Reference:

    Richard Barrett, Michael Berry, Tony Chan, James Demmel,
    June Donato, Jack Dongarra, Victor Eijkhout, Roidan Pozo,
    Charles Romine, Henk van der Vorst,
    Templates for the Solution of Linear Systems:
    Building Blocks for Iterative Methods,
    SIAM, 1994,
    ISBN: 0898714710,
    LC: QA297.8.T45.

    Tim Kelley,
    Iterative Methods for Linear and Nonlinear Equations,
    SIAM, 2004,
    ISBN: 0898713528,
    LC: QA297.8.K45.

    Yousef Saad,
    Iterative Methods for Sparse Linear Systems,
    Second Edition,
    SIAM, 20003,
    ISBN: 0898715342,
    LC: QA188.S17.

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[N+1], JA[NZ_NUM], the row and column indices
    of the matrix values.  The row vector has been compressed.

    Input, double A[NZ_NUM], the matrix values.

    Input, double X[N], the vector to be multiplied by A'.

    Output, double W[N], the value of A'*X.
*/
{
  int i;
  int k;
  int k1;
  int k2;

  for ( i = 0; i < n; i++ )
  {
    w[i] = 0.0;
    k1 = ia[i];
    k2 = ia[i+1];
    for ( k = k1; k < k2; k++ )
    {
      w[ja[k]] = w[ja[k]] + a[k] * x[i];
    }
  }
  return;
}
/******************************************************************************/

void atx_st ( int n, int nz_num, int ia[], int ja[], double a[], double x[],
  double w[] )

/******************************************************************************/
/*
  Purpose:

    ATX_ST computes A'*x for a matrix stored in sparse triplet form.

  Discussion:

    The matrix A is assumed to be stored in sparse triplet format.  Only
    the nonzero entries of A are stored.  For instance, the K-th nonzero
    entry in the matrix is stored by:

      A(K) = value of entry,
      IA(K) = row of entry,
      JA(K) = column of entry.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    17 July 2007

  Author:

    Lili Ju

  Reference:

    Richard Barrett, Michael Berry, Tony Chan, James Demmel,
    June Donato, Jack Dongarra, Victor Eijkhout, Roidan Pozo,
    Charles Romine, Henk van der Vorst,
    Templates for the Solution of Linear Systems:
    Building Blocks for Iterative Methods,
    SIAM, 1994,
    ISBN: 0898714710,
    LC: QA297.8.T45.

    Tim Kelley,
    Iterative Methods for Linear and Nonlinear Equations,
    SIAM, 2004,
    ISBN: 0898713528,
    LC: QA297.8.K45.

    Yousef Saad,
    Iterative Methods for Sparse Linear Systems,
    Second Edition,
    SIAM, 20003,
    ISBN: 0898715342,
    LC: QA188.S17.

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[NZ_NUM], JA[NZ_NUM], the row and column indices
    of the matrix values.

    Input, double A[NZ_NUM], the matrix values.

    Input, double X[N], the vector to be multiplied by A'.

    Output, double W[N], the value of A'*X.
*/
{
  int i;
  int j;
  int k;

  for ( i = 0; i < n; i++ )
  {
    w[i] = 0.0;
  }

  for ( k = 0; k < nz_num; k++ )
  {
    i = ia[k];
    j = ja[k];
    w[j] = w[j] + a[k] * x[i];
  }

  return;
}
/******************************************************************************/

void ax_cr ( int n, int nz_num, int ia[], int ja[], double a[], double x[],
  double w[] )

/******************************************************************************/
/*
  Purpose:

    AX_CR computes A*x for a matrix stored in sparse compressed row form.

  Discussion:

    The matrix A is assumed to be stored in compressed row format.  Only
    the nonzero entries of A are stored.  The vector JA stores the
    column index of the nonzero value.  The nonzero values are sorted
    by row, and the compressed row vector IA then has the property that
    the entries in A and JA that correspond to row I occur in indices
    IA[I] through IA[I+1]-1.

    For this version of MGMRES, the row and column indices are assumed
    to use the C/C++ convention, in which indexing begins at 0.

    If your index vectors IA and JA are set up so that indexing is based 
    at 1, each use of those vectors should be shifted down by 1.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    17 July 2007

  Author:

    Lili Ju

  Reference:

    Richard Barrett, Michael Berry, Tony Chan, James Demmel,
    June Donato, Jack Dongarra, Victor Eijkhout, Roidan Pozo,
    Charles Romine, Henk van der Vorst,
    Templates for the Solution of Linear Systems:
    Building Blocks for Iterative Methods,
    SIAM, 1994,
    ISBN: 0898714710,
    LC: QA297.8.T45.

    Tim Kelley,
    Iterative Methods for Linear and Nonlinear Equations,
    SIAM, 2004,
    ISBN: 0898713528,
    LC: QA297.8.K45.

    Yousef Saad,
    Iterative Methods for Sparse Linear Systems,
    Second Edition,
    SIAM, 20003,
    ISBN: 0898715342,
    LC: QA188.S17.

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[N+1], JA[NZ_NUM], the row and column indices
    of the matrix values.  The row vector has been compressed.

    Input, double A[NZ_NUM], the matrix values.

    Input, double X[N], the vector to be multiplied by A.

    Output, double W[N], the value of A*X.
*/
{
  int i;
  int k;
  int k1;
  int k2;

  for ( i = 0; i < n; i++ )
  {
    w[i] = 0.0;
    k1 = ia[i];
    k2 = ia[i+1];
    for ( k = k1; k < k2; k++ )
    {
      w[i] = w[i] + a[k] * x[ja[k]];
    }
  }
  return;
}
/******************************************************************************/

void ax_st ( int n, int nz_num, int ia[], int ja[], double a[], double x[],
  double w[] )

/******************************************************************************/
/*
  Purpose:

    AX_ST computes A*x for a matrix stored in sparse triplet form.

  Discussion:

    The matrix A is assumed to be stored in sparse triplet format.  Only
    the nonzero entries of A are stored.  For instance, the K-th nonzero
    entry in the matrix is stored by:

      A(K) = value of entry,
      IA(K) = row of entry,
      JA(K) = column of entry.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    17 July 2007

  Author:

    Lili Ju

  Reference:

    Richard Barrett, Michael Berry, Tony Chan, James Demmel,
    June Donato, Jack Dongarra, Victor Eijkhout, Roidan Pozo,
    Charles Romine, Henk van der Vorst,
    Templates for the Solution of Linear Systems:
    Building Blocks for Iterative Methods,
    SIAM, 1994,
    ISBN: 0898714710,
    LC: QA297.8.T45.

    Tim Kelley,
    Iterative Methods for Linear and Nonlinear Equations,
    SIAM, 2004,
    ISBN: 0898713528,
    LC: QA297.8.K45.

    Yousef Saad,
    Iterative Methods for Sparse Linear Systems,
    Second Edition,
    SIAM, 20003,
    ISBN: 0898715342,
    LC: QA188.S17.

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[NZ_NUM], JA[NZ_NUM], the row and column indices
    of the matrix values.

    Input, double A[NZ_NUM], the matrix values.

    Input, double X[N], the vector to be multiplied by A.

    Output, double W[N], the value of A*X.
*/
{
  int i;
  int j;
  int k;

  for ( i = 0; i < n; i++ )
  {
    w[i] = 0.0;
  }

  for ( k = 0; k < nz_num; k++ )
  {
    i = ia[k];
    j = ja[k];
    w[i] = w[i] + a[k] * x[j];
  }

  return;
}
/******************************************************************************/

void diagonal_pointer_cr ( int n, int nz_num, int ia[], int ja[], int ua[] )

/******************************************************************************/
/*
  Purpose:

    DIAGONAL_POINTER_CR finds diagonal entries in a sparse compressed row matrix.

  Discussion:

    The matrix A is assumed to be stored in compressed row format.  Only
    the nonzero entries of A are stored.  The vector JA stores the
    column index of the nonzero value.  The nonzero values are sorted
    by row, and the compressed row vector IA then has the property that
    the entries in A and JA that correspond to row I occur in indices
    IA[I] through IA[I+1]-1.

    The array UA can be used to locate the diagonal elements of the matrix.

    It is assumed that every row of the matrix includes a diagonal element,
    and that the elements of each row have been ascending sorted.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    17 July 2007

  Author:

    Lili Ju

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[N+1], JA[NZ_NUM], the row and column indices
    of the matrix values.  The row vector has been compressed.  On output,
    the order of the entries of JA may have changed because of the sorting.

    Output, int UA[N], the index of the diagonal element of each row.
*/
{
  int i;
  int j;
  int j1;
  int j2;

  for ( i = 0; i < n; i++ )
  {
    ua[i] = -1;
    j1 = ia[i];
    j2 = ia[i+1];

    for ( j = j1; j < j2; j++ )
    {
      if ( ja[j] == i ) 
      {
        ua[i] = j;
      }
    }

  }
  return;
}
/******************************************************************************/

double **dmatrix ( int nrl, int nrh, int ncl, int nch )

/******************************************************************************/
/*
  Purpose:

    DMATRIX allocates a double matrix.

  Discussion:

    The matrix will have a subscript range m[nrl...nrh][ncl...nch] .

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    10 August 2006

  Author:

    Lili Ju

  Parameters:

    Input, int NRL, NRH, the low and high row indices.

    Input, int NCL, NCH, the low and high column indices.

    Output, double **DMATRIX, a doubly-dimensioned array with
    the requested row and column ranges.
*/
{
  int i;
  double **m;
  int nrow = nrh - nrl + 1;
  int ncol = nch - ncl + 1;
/* 
  Allocate pointers to the rows.
*/
  m = ( double ** ) malloc ( (size_t) ( ( nrow + 1 ) * sizeof ( double* ) ) );

  if ( ! m ) 
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "DMATRIX - Fatal error!\n" );
    fprintf ( stderr, "  Failure allocating pointers to rows.\n");
    exit ( 1 );
  }
  m = m + 1;
  m = m - nrl;
/* 
  Allocate each row and set pointers to them.
*/
  m[nrl] = ( double * ) malloc ( (size_t) ( ( nrow * ncol + 1 ) * sizeof ( double ) ) );

  if ( ! m[nrl] ) 
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "DMATRIX - Fatal error!\n" );
    fprintf ( stderr, "  Failure allocating rows.\n");
    exit ( 1 );
  }
  m[nrl] = m[nrl] + 1;
  m[nrl] = m[nrl] - ncl;

  for ( i = nrl + 1; i <= nrh; i++ ) 
  { 
    m[i] = m[i-1] + ncol;
  }
/* 
  Return the pointer to the array of pointers to the rows;
*/
  return m;
}
/******************************************************************************/

void free_dmatrix ( double **m, int nrl, int nrh, int ncl, int nch )

/******************************************************************************/
/*
  Purpose:

    FREE_DMATRIX frees a double matrix allocated by DMATRIX.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    10 August 2006

  Author:

    Lili Ju

  Parameters:

    Input, int NRL, NRH, the low and high row indices.

    Input, int NCL, NCH, the low and high column indices.

    Input, double **M, the pointer to the doubly-dimensioned array,
    previously created by a call to DMATRIX.
*/
{
  free ( ( char * ) ( m[nrl] + ncl - 1 ) );
  free ( ( char * ) ( m + nrl - 1 ) );

  return;
}
/******************************************************************************/

void ilu_cr ( int n, int nz_num, int ia[], int ja[], double a[], int ua[],
  double l[] )

/******************************************************************************/
/*
  Purpose:

    ILU_CR computes the incomplete LU factorization of a matrix.

  Discussion:

    The matrix A is assumed to be stored in compressed row format.  Only
    the nonzero entries of A are stored.  The vector JA stores the
    column index of the nonzero value.  The nonzero values are sorted
    by row, and the compressed row vector IA then has the property that
    the entries in A and JA that correspond to row I occur in indices
    IA[I] through IA[I+1]-1.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    18 July 2007

  Author:

    Lili Ju

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[N+1], JA[NZ_NUM], the row and column indices
    of the matrix values.  The row vector has been compressed.

    Input, double A[NZ_NUM], the matrix values.

    Input, int UA[N], the index of the diagonal element of each row.

    Output, double L[NZ_NUM], the ILU factorization of A.
*/
{
  int *iw;
  int i;
  int j;
  int jj;
  int jrow;
  int jw;
  int k;
  double tl;

  iw = ( int * ) malloc ( n * sizeof ( int ) );
/*
  Copy A.
*/
  for ( k = 0; k < nz_num; k++ ) 
  {
    l[k] = a[k];
  }

  for ( i = 0; i < n; i++ ) 
  {
/*
  IW points to the nonzero entries in row I.
*/
    for ( j = 0; j < n; j++ )
    {
      iw[j] = -1;
    }

    for ( k = ia[i]; k <= ia[i+1] - 1; k++ ) 
    {
      iw[ja[k]] = k;
    }

    j = ia[i];
    do 
    {
      jrow = ja[j];
      if ( i <= jrow )
      {
        break;
      }
      tl = l[j] * l[ua[jrow]];
      l[j] = tl;
      for ( jj = ua[jrow] + 1; jj <= ia[jrow+1] - 1; jj++ ) 
      {
        jw = iw[ja[jj]];
        if ( jw != -1 ) 
        {
          l[jw] = l[jw] - tl * l[jj];
        }
      }
      j = j + 1;
    } while ( j <= ia[i+1] - 1 );

    ua[i] = j;

    if ( jrow != i ) 
    {
      printf ( "\n" );
      printf ( "ILU_CR - Fatal error!\n" );
      printf ( "  JROW != I\n" );
      printf ( "  JROW = %d\n", jrow );
      printf ( "  I    = %d\n", i );
      exit ( 1 );
    }

    if ( l[j] == 0.0 ) 
    {
      fprintf ( stderr, "\n" );
      fprintf ( stderr, "ILU_CR - Fatal error!\n" );
      fprintf ( stderr, "  Zero pivot on step I = %d\n", i );
      fprintf ( stderr, "  L[%d] = 0.0\n", j );
      exit ( 1 );
    }

    l[j] = 1.0 / l[j];
  }

  for ( k = 0; k < n; k++ ) 
  {
    l[ua[k]] = 1.0 / l[ua[k]];
  }
/*
  Free memory.
*/
  free ( iw );

  return;
}
/******************************************************************************/

void lus_cr ( int n, int nz_num, int ia[], int ja[], double l[], int ua[], 
  double r[], double z[] )

/******************************************************************************/
/*
  Purpose:

    LUS_CR applies the incomplete LU preconditioner.

  Discussion:

    The linear system M * Z = R is solved for Z.  M is the incomplete
    LU preconditioner matrix, and R is a vector supplied by the user.
    So essentially, we're solving L * U * Z = R.

    The matrix A is assumed to be stored in compressed row format.  Only
    the nonzero entries of A are stored.  The vector JA stores the
    column index of the nonzero value.  The nonzero values are sorted
    by row, and the compressed row vector IA then has the property that
    the entries in A and JA that correspond to row I occur in indices
    IA[I] through IA[I+1]-1.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    18 July 2007

  Author:

    Lili Ju

  Parameters:

    Input, int N, the order of the system.

    Input, int NZ_NUM, the number of nonzeros.

    Input, int IA[N+1], JA[NZ_NUM], the row and column indices
    of the matrix values.  The row vector has been compressed.

    Input, double L[NZ_NUM], the matrix values.

    Input, int UA[N], the index of the diagonal element of each row.

    Input, double R[N], the right hand side.

    Output, double Z[N], the solution of the system M * Z = R.
*/
{
  int i;
  int j;
  double *w;

  w = ( double * ) malloc ( n * sizeof ( double ) );
/*
  Copy R in.
*/
  for ( i = 0; i < n; i++ )
  {
    w[i] = r[i];
  }
/*
  Solve L * w = w where L is unit lower triangular.
*/
  for ( i = 1; i < n; i++ )
  {
    for ( j = ia[i]; j < ua[i]; j++ )
    {
      w[i] = w[i] - l[j] * w[ja[j]];
    }
  }
/*
  Solve U * w = w, where U is upper triangular.
*/
  for ( i = n - 1; 0 <= i; i-- ) 
  {
    for ( j = ua[i] + 1; j < ia[i+1]; j++ ) 
    {
      w[i] = w[i] - l[j] * w[ja[j]];
    }
    w[i] = w[i] / l[ua[i]];
  }
/*
  Copy Z out.
*/
  for ( i = 0; i < n; i++ )
  {
    z[i] = w[i];
  }
/*
  Free memory.
*/
  free ( w );

  return;
}
/******************************************************************************/

void mgmres_st ( int n, int nz_num, int ia[], int ja[], double a[], 
  double x[], double rhs[], int itr_max, int mr, double tol_abs, 
  double tol_rel )

/******************************************************************************/
/*
  Purpose:

    MGMRES_ST applies restarted GMRES to a matrix in sparse triplet form.

  Discussion:

    The matrix A is assumed to be stored in sparse triplet form.  Only 
    the nonzero entries of A are stored.  For instance, the 
    K-th nonzero entry in the matrix is stored by:

      A(K) = value of entry,
      IA(K) = row of entry,
      JA(K) = column of entry.

    Thanks to Jesus Pueblas Sanchez-Guerra for supplying two
    corrections to the code on 31 May 2007.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    18 July 2007

  Author:

    Lili Ju

  Reference:

    Richard Barrett, Michael Berry, Tony Chan, James Demmel,
    June Donato, Jack Dongarra, Victor Eijkhout, Roidan Pozo,
    Charles Romine, Henk van der Vorst,
    Templates for the Solution of Linear Systems:
    Building Blocks for Iterative Methods,
    SIAM, 1994.
    ISBN: 0898714710,
    LC: QA297.8.T45.

    Tim Kelley,
    Iterative Methods for Linear and Nonlinear Equations,
    SIAM, 2004,
    ISBN: 0898713528,
    LC: QA297.8.K45.

    Yousef Saad,
    Iterative Methods for Sparse Linear Systems,
    Second Edition,
    SIAM, 2003,
    ISBN: 0898715342,
    LC: QA188.S17.

  Parameters:

    Input, int N, the order of the linear system.

    Input, int NZ_NUM, the number of nonzero matrix values.

    Input, int IA[NZ_NUM], JA[NZ_NUM], the row and column indices of 
    the matrix values.

    Input, double A[], the matrix values.

    Input/output, double X[N]; on input, an approximation to
    the solution.  On output, an improved approximation.

    Input, double RHS[N], the right hand side of the linear system.

    Input, int ITR_MAX, the maximum number of (outer) iterations to take.

    Input, int MR, the maximum number of (inner) iterations to take.
    MR must be less than N.

    Input, double TOL_ABS, an absolute tolerance applied to the
    current residual.

    Input, double TOL_REL, a relative tolerance comparing the
    current residual to the initial residual.
*/
{
  double av;
  double *c;
  double delta = 1.0e-03;
  double *g;
  double **h;
  double htmp;
  int i;
  int itr;
  int itr_used;
  int j;
  int k;
  int k_copy;
  double mu;
  double *r;
  double rho;
  double rho_tol;
  double *s;
  double **v;
  int verbose = 0;
  double *y;

  if ( n < mr )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "MGMRES_ST - Fatal error!\n" );
    fprintf ( stderr, "  N < MR.\n" );
    fprintf ( stderr, "  N = %d\n", n );
    fprintf ( stderr, "  MR = %d\n", mr );
    exit ( 1 );
  }

  itr_used = 0;

  c = ( double * ) malloc ( mr * sizeof ( double ) );
  g = ( double * ) malloc ( ( mr + 1 ) * sizeof ( double ) );
  h = dmatrix(0,mr,0,mr-1);
  r = ( double * ) malloc ( n * sizeof ( double ) );
  s = ( double * ) malloc ( mr * sizeof ( double ) );
  v = dmatrix(0,mr,0,n-1);
  y = ( double * ) malloc ( ( mr + 1 ) * sizeof ( double ) );

  for ( itr = 0; itr < itr_max; itr++ ) 
  {
    ax_st ( n, nz_num, ia, ja, a, x, r );

    for ( i = 0; i < n; i++ )
    {
      r[i] = rhs[i] - r[i];
    }

    rho = sqrt ( r8vec_dot ( n, r, r ) );

    if ( verbose )
    {
      printf ( "  ITR = %8d  Residual = %e\n", itr, rho );
    }

    if ( itr == 0 )
    {
      rho_tol = rho * tol_rel;
    }

    for ( i = 0; i < n; i++ )
    {
      v[0][i] = r[i] / rho;
    }

    g[0] = rho;
    for ( i = 1; i < mr + 1; i++ )
    {
      g[i] = 0.0;
    }

    for ( i = 0; i < mr + 1; i++ )
    {
      for ( j = 0; j < mr; j++ ) 
      {
        h[i][j] = 0.0;
      }
    }

    for ( k = 0; k < mr; k++ )
    {
      k_copy = k;

      ax_st ( n, nz_num, ia, ja, a, v[k], v[k+1] );

      av = sqrt ( r8vec_dot ( n, v[k+1], v[k+1] ) );

      for ( j = 0; j < k+1; j++ )
      {
        h[j][k] = r8vec_dot ( n, v[k+1], v[j] );
        for ( i = 0; i < n; i++ ) 
        {
          v[k+1][i] = v[k+1][i] - h[j][k] * v[j][i];
        }
      }

      h[k+1][k] = sqrt ( r8vec_dot ( n, v[k+1], v[k+1] ) );

      if ( ( av + delta * h[k+1][k] ) == av )
      {
        for ( j = 0; j < k+1; j++ )
        {
          htmp = r8vec_dot ( n, v[k+1], v[j] );
          h[j][k] = h[j][k] + htmp;
          for ( i = 0; i < n; i++ ) 
          {
            v[k+1][i] = v[k+1][i] - htmp * v[j][i];
          }
        }
        h[k+1][k] = sqrt ( r8vec_dot ( n, v[k+1], v[k+1] ) );
      }

      if ( h[k+1][k] != 0.0 )
      {
        for ( i = 0; i < n; i++ ) 
        {
          v[k+1][i] = v[k+1][i] / h[k+1][k];
        }
      }

      if ( 0 < k )
      {
        for ( i = 0; i < k + 2; i++ )
        {
          y[i] = h[i][k];
        }
        for ( j = 0; j < k; j++ ) 
        {
          mult_givens ( c[j], s[j], j, y );
        }
        for ( i = 0; i < k + 2; i++ ) 
        {
          h[i][k] = y[i];
        }
      }

      mu = sqrt ( h[k][k] * h[k][k] + h[k+1][k] * h[k+1][k] );
      c[k] = h[k][k] / mu;
      s[k] = -h[k+1][k] / mu;
      h[k][k] = c[k] * h[k][k] - s[k] * h[k+1][k];
      h[k+1][k] = 0.0;
      mult_givens ( c[k], s[k], k, g );

      rho = fabs ( g[k+1] );

      itr_used = itr_used + 1;

      if ( verbose )
      {
        printf ( "  K =   %8d  Residual = %e\n", k, rho );
      }

      if ( rho <= rho_tol && rho <= tol_abs )
      {
        break;
      }
    }

    k = k_copy;

    y[k] = g[k] / h[k][k];
    for ( i = k - 1; 0 <= i; i-- )
    {
      y[i] = g[i];
      for ( j = i+1; j < k + 1; j++ ) 
      {
        y[i] = y[i] - h[i][j] * y[j];
      }
      y[i] = y[i] / h[i][i];
    }

    for ( i = 0; i < n; i++ )
    {
      for ( j = 0; j < k + 1; j++ )
      {
        x[i] = x[i] + v[j][i] * y[j];
      }
    }

    if ( rho <= rho_tol && rho <= tol_abs ) 
    {
      break;
    }
  }
  if ( verbose )
  {
    printf ( "\n" );
    printf ( "MGMRES_ST:\n" );
    printf ( "  Iterations = %d\n", itr_used );
    printf ( "  Final residual = %e\n", rho );
  }
/*
  Free memory.
*/
  free ( c );
  free ( g );
  free_dmatrix ( h, 0, mr, 0, mr - 1 );
  free ( r );
  free ( s );
  free_dmatrix ( v, 0, mr, 0, n - 1 );
  free ( y );

  return;
}
/******************************************************************************/

void pmgmres_ilu_cr ( int n, int nz_num, int ia[], int ja[], double a[], 
  double x[], double rhs[], int itr_max, int mr, double tol_abs, 
  double tol_rel )

/******************************************************************************/
/*
  Purpose:

    PMGMRES_ILU_CR applies the preconditioned restarted GMRES algorithm.

  Discussion:

    The matrix A is assumed to be stored in compressed row format.  Only
    the nonzero entries of A are stored.  The vector JA stores the
    column index of the nonzero value.  The nonzero values are sorted
    by row, and the compressed row vector IA then has the property that
    the entries in A and JA that correspond to row I occur in indices
    IA[I] through IA[I+1]-1.

    This routine uses the incomplete LU decomposition for the
    preconditioning.  This preconditioner requires that the sparse
    matrix data structure supplies a storage position for each diagonal
    element of the matrix A, and that each diagonal element of the
    matrix A is not zero.

    Thanks to Jesus Pueblas Sanchez-Guerra for supplying two
    corrections to the code on 31 May 2007.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    18 July 2007

  Author:

    Lili Ju

  Reference:

    Richard Barrett, Michael Berry, Tony Chan, James Demmel,
    June Donato, Jack Dongarra, Victor Eijkhout, Roidan Pozo,
    Charles Romine, Henk van der Vorst,
    Templates for the Solution of Linear Systems:
    Building Blocks for Iterative Methods,
    SIAM, 1994.
    ISBN: 0898714710,
    LC: QA297.8.T45.

    Tim Kelley,
    Iterative Methods for Linear and Nonlinear Equations,
    SIAM, 2004,
    ISBN: 0898713528,
    LC: QA297.8.K45.

    Yousef Saad,
    Iterative Methods for Sparse Linear Systems,
    Second Edition,
    SIAM, 2003,
    ISBN: 0898715342,
    LC: QA188.S17.

  Parameters:

    Input, int N, the order of the linear system.

    Input, int NZ_NUM, the number of nonzero matrix values.

    Input, int IA[N+1], JA[NZ_NUM], the row and column indices
    of the matrix values.  The row vector has been compressed.

    Input, double A[NZ_NUM], the matrix values.

    Input/output, double X[N]; on input, an approximation to
    the solution.  On output, an improved approximation.

    Input, double RHS[N], the right hand side of the linear system.

    Input, int ITR_MAX, the maximum number of (outer) iterations to take.

    Input, int MR, the maximum number of (inner) iterations to take.
    MR must be less than N.

    Input, double TOL_ABS, an absolute tolerance applied to the
    current residual.

    Input, double TOL_REL, a relative tolerance comparing the
    current residual to the initial residual.
*/
{
  double av;
  double *c;
  double delta = 1.0e-03;
  double *g;
  double **h;
  double htmp;
  int i;
  int itr;
  int itr_used;
  int j;
  int k;
  int k_copy;
  double *l;
  double mu;
  double *r;
  double rho;
  double rho_tol;
  double *s;
  int *ua;
  double **v;
  int verbose = 1;
  double *y;

  itr_used = 0;

  c = ( double * ) malloc ( ( mr + 1 ) * sizeof ( double ) );
  g = ( double * ) malloc ( ( mr + 1 ) * sizeof ( double ) );
  h = dmatrix ( 0, mr, 0, mr-1 );
  l = ( double * ) malloc ( ( ia[n] + 1 ) * sizeof ( double ) );
  r = ( double * ) malloc ( n * sizeof ( double ) );
  s = ( double * ) malloc ( ( mr + 1 ) * sizeof ( double ) );
  ua = ( int * ) malloc ( n * sizeof ( int ) );
  v = dmatrix ( 0, mr, 0, n-1 );
  y = ( double * ) malloc ( ( mr + 1 ) * sizeof ( double ) );

  rearrange_cr ( n, nz_num, ia, ja, a );

  diagonal_pointer_cr ( n, nz_num, ia, ja, ua );

  ilu_cr ( n, nz_num, ia, ja, a, ua, l );

  if ( verbose )
  {
    printf ( "\n" );
    printf ( "PMGMRES_ILU_CR\n" );
    printf ( "  Number of unknowns = %d\n", n );
  }

  for ( itr = 0; itr < itr_max; itr++ ) 
  {
    ax_cr ( n, nz_num, ia, ja, a, x, r );

    for ( i = 0; i < n; i++ ) 
    {
      r[i] = rhs[i] - r[i];
    }

    lus_cr ( n, nz_num, ia, ja, l, ua, r, r );

    rho = sqrt ( r8vec_dot ( n, r, r ) );

    if ( verbose )
    {
      printf ( "  ITR = %d  Residual = %e\n", itr, rho );
    }

    if ( itr == 0 )
    {
      rho_tol = rho * tol_rel;
    }

    for ( i = 0; i < n; i++ ) 
    {
      v[0][i] = r[i] / rho;
    }

    g[0] = rho;
    for ( i = 1; i < mr + 1; i++ ) 
    {
      g[i] = 0.0;
    }

    for ( i = 0; i < mr + 1; i++ ) 
    {
      for ( j = 0; j < mr; j++ ) 
      {
        h[i][j] = 0.0;
      }
    }

    for ( k = 0; k < mr; k++ )
    {
      k_copy = k;

      ax_cr ( n, nz_num, ia, ja, a, v[k], v[k+1] ); 

      lus_cr ( n, nz_num, ia, ja, l, ua, v[k+1], v[k+1] );

      av = sqrt ( r8vec_dot ( n, v[k+1], v[k+1] ) );

      for ( j = 0; j <= k; j++ ) 
      {
        h[j][k] = r8vec_dot ( n, v[k+1], v[j] );
        for ( i = 0; i < n; i++ ) 
        {
          v[k+1][i] = v[k+1][i] - h[j][k] * v[j][i];
        }
      }
      h[k+1][k] = sqrt ( r8vec_dot ( n, v[k+1], v[k+1] ) );

      if ( ( av + delta * h[k+1][k]) == av ) 
      {
        for ( j = 0; j < k + 1; j++ )
        {
          htmp = r8vec_dot ( n, v[k+1], v[j] );
          h[j][k] = h[j][k] + htmp;
          for ( i = 0; i < n; i++ ) 
          {
            v[k+1][i] = v[k+1][i] - htmp * v[j][i];
          }
        }
        h[k+1][k] = sqrt ( r8vec_dot ( n, v[k+1], v[k+1] ) );
      }

      if ( h[k+1][k] != 0.0 )
      {
        for ( i = 0; i < n; i++ )
        {
          v[k+1][i] = v[k+1][i] / h[k+1][k];
        } 
      }

      if ( 0 < k )  
      {
        for ( i = 0; i < k + 2; i++ ) 
        {
          y[i] = h[i][k];
        }
        for ( j = 0; j < k; j++ ) 
        {
          mult_givens ( c[j], s[j], j, y );
        }
        for ( i = 0; i < k + 2; i++ )
        {
          h[i][k] = y[i];
        }
      }
      mu = sqrt ( h[k][k] * h[k][k] + h[k+1][k] * h[k+1][k] );

      c[k] = h[k][k] / mu;
      s[k] = -h[k+1][k] / mu;
      h[k][k] = c[k] * h[k][k] - s[k] * h[k+1][k];
      h[k+1][k] = 0.0;
      mult_givens ( c[k], s[k], k, g );

      rho = fabs ( g[k+1] );

      itr_used = itr_used + 1;

      if ( verbose )
      {
        printf ( "  K   = %d  Residual = %e\n", k, rho );
      }

      if ( rho <= rho_tol && rho <= tol_abs )
      {
        break;
      }
    }

    k = k_copy;

    y[k] = g[k] / h[k][k];
    for ( i = k - 1; 0 <= i; i-- )
    {
      y[i] = g[i];
      for ( j = i + 1; j < k + 1; j++ ) 
      {
        y[i] = y[i] - h[i][j] * y[j];
      }
      y[i] = y[i] / h[i][i];
    }
    for ( i = 0; i < n; i++ )
    {
      for ( j = 0; j < k + 1; j++ )
      {
        x[i] = x[i] + v[j][i] * y[j];
      }
    }

    if ( rho <= rho_tol && rho <= tol_abs )
    {
      break;
    }
  }

  if ( verbose )
  {
    printf ( "\n" );
    printf ( "PMGMRES_ILU_CR:\n" );
    printf ( "  Iterations = %d\n", itr_used );
    printf ( "  Final residual = %e\n", rho );
  }
/*
  Free memory.
*/
  free ( c );
  free ( g );
  free_dmatrix ( h, 0, mr, 0, mr - 1 );
  free ( l );
  free ( r );
  free ( s );
  free ( ua );
  free_dmatrix ( v, 0, mr, 0, n - 1 );
  free ( y );

  return;
}
/******************************************************************************/

/*
    Solves a sparse linear system using least square error
    in the form ofa compressed sparse row representation.

    Parameters
    ----------
    - n = matrix size (length of rhs)
    - na = entries size (length of a)
    - a(k) = value of entry [left hand side]
    - ia(k) = row of entry
    - ja(k) = column of entry
    - rhs = right hand side of the equation
    - x = solution
*/
void solveSparseSystem(int n,
                       int na,
                       double *a,
                       int *ia,
                       int *ja,
                       double *rhs,
                       double *x) {
    
    int mr = 2000;
    int inter_max = 1;

    mgmres_st(n, na, ia, ja, a, x, rhs, inter_max, mr, 1e-8, 1e-8);

}

void calculateProfiles(double delta,
                       double A,
                       double B,
                       double Psi,
                       double Ctau1,
                       double Ctau2,
                       struct FreestreamParameters *freestream,
                       struct ProfileParameters *profiles)
{

    /* Parameters */
    int i;                             // loop
    int flow_type;                     // laminar or turbulent
    double delta_eta;                  // eta step or first step
    double Re_delta;                   // reynolds number
    double f0, f1, f2, f3;             // laminar curves
    double mu_mui;                     // viscosity ratio
    double h_hi;                       // enthalpy ratio
    double epsilon_line;               // thermodynamic aux parameter
    double Utau, Wtau, qtau;           // turbulent shear velocities
    double y_plus_1;                   // first element height
    double u_plus_max;                 // u_plus(delta_plus)
    double g0;                         // outer layer profile
    double dg0deta;                    // outer layer profile derivative
    double delta_plus;                 // dimensionless turbulent boundary layer height
    double Upsilon, K;                 // turbulent outer layer parameters
    double *u_plus, *y_plus;           // turbulent law of the wall profiles
    double k, C;                       // law of the wall parameters
    double u_min, y_min, u_max, y_max; // buffer region interpolation limits
    double log_y_min, log_y_max;       // log of the interpolation limits
    double a, b, c, t;                 // interpolation parameters
    double exp_ratio;                  // expansion ratio
    double eta2, eta3, eta4, eta5;     // power of eta

    /* Initilize */
    if (sqrt(pow(Ctau1, 2) + pow(Ctau2, 2)) > CTAU_CRIT)
    {
        flow_type = 1;
    }
    else
    {
        flow_type = 0;
    };
    Re_delta = freestream->velocity * freestream->density * delta / freestream->viscosity;
    u_plus = (double *)malloc(profiles->n * sizeof(double));
    y_plus = (double *)malloc(profiles->n * sizeof(double));
    k = 0.41;
    C = 5.0;
    u_min = 5.0;
    y_min = 5.0;
    u_max = 17.922725284263503;
    y_max = 200;
    log_y_min = log10(y_min);
    log_y_max = log10(y_max);
    a = u_min + 10 * log_y_min * log(10) * 0.26957378;
    b = 14.2135593;
    c = u_max - (1 / (k * log10(M_E))) * 0.51958278;
    y_plus_1 = 0.1;
    exp_ratio = 0.95;
    Utau = A / ((pow(pow(A, 2) + pow(B, 2), 0.25) * sqrt(Re_delta)) + 1e-10);
    Wtau = B / ((pow(pow(A, 2) + pow(B, 2), 0.25) * sqrt(Re_delta)) + 1e-10);
    epsilon_line = 0.2 * pow(freestream->mach, 2);

    /* Define eta */
    if (flow_type == LAMINAR_FLOW)
    {

        delta_eta = 1 / ((double)profiles->n - 1);

        for (i = 0; i < profiles->n; i++)
        {
            profiles->eta[i] = i * delta_eta;
        }
    }
    else
    {

        h_hi = 1 + epsilon_line;
        mu_mui = pow(h_hi, 1.5) * 2 / (h_hi + 1);
        delta_plus = sqrt(Re_delta) * (1 / mu_mui) * (1 / h_hi) * pow(pow(A, 2) + pow(B, 2), 0.25);

        if (delta_plus / ((double)profiles->n - 1) > y_plus_1)
        { // Geoemtric distribution

            // Find expansion ratio
            find_exp_ratio(delta_plus, y_plus_1, (double)profiles->n - 1, &exp_ratio);

            for (i = 0; i < profiles->n; i++)
            {
                if (i == 0)
                {
                    y_plus[i] = 0.0;
                }
                else if (i == profiles->n - 1)
                {
                    y_plus[i] = delta_plus;
                }
                else
                {
                    y_plus[i] = y_plus[i - 1] + y_plus_1 * pow(exp_ratio, i - 1);
                }
                profiles->eta[i] = y_plus[i] / delta_plus;
            }
        }
        else
        { // Linear distribution

            delta_eta = 1 / ((double)profiles->n - 1);

            for (i = 0; i < profiles->n; i++)
            {
                profiles->eta[i] = i * delta_eta;
                y_plus[i] = delta_plus * profiles->eta[i];
            }
        }
    }

    /* Create profiles */
    if (flow_type == LAMINAR_FLOW)
    {

        // Velocities
        for (i = 0; i < profiles->n; i++)
        {

            eta2 = pow(profiles->eta[i], 2);
            eta3 = pow(profiles->eta[i], 3);
            eta4 = pow(profiles->eta[i], 4);
            eta5 = pow(profiles->eta[i], 5);

            f0 = 6 * eta2 - 8 * eta3 + 3 * eta4;
            f1 = profiles->eta[i] - 3 * eta2 + 3 * eta3 - eta4;
            f2 = (profiles->eta[i] - 4 * eta2 + 6 * eta3 - 4 * eta4 + eta5) * pow(1 - profiles->eta[i], 2);
            f3 = (eta2 - 3 * eta3 + 3 * eta4 - eta5) * pow(1 - profiles->eta[i], 2);

            profiles->U[i] = A * (1 - 0.6 * (A - 3) * eta3) * f1 + f0;
            profiles->W[i] = B * f2 + Psi * f3;

            profiles->R[i] = 1 / (1 + epsilon_line * (1 - pow(profiles->U[i], 2) - pow(profiles->W[i], 2)));
        }

        // Gradient and Shear stress
        for (i = 0; i < profiles->n; i++)
        {

            f0 = 12 * profiles->eta[i] - 24 * pow(profiles->eta[i], 2) + 12 * pow(profiles->eta[i], 3);
            f1 = 1 - 6 * profiles->eta[i] + 9 * pow(profiles->eta[i], 2) - 4 * pow(profiles->eta[i], 3);
            f2 = (1 - 8 * profiles->eta[i] + 18 * pow(profiles->eta[i], 2) - 16 * pow(profiles->eta[i], 3) + 5 * pow(profiles->eta[i], 4)) * pow(1 - profiles->eta[i], 2) - 2 * (1 - profiles->eta[i]) * (profiles->eta[i] - 3 * pow(profiles->eta[i], 2) + 3 * pow(profiles->eta[i], 3) - pow(profiles->eta[i], 4));
            f3 = (2 * profiles->eta[i] - 9 * pow(profiles->eta[i], 2) + 12 * pow(profiles->eta[i], 3) - 5 * pow(profiles->eta[i], 4)) * pow(1 - profiles->eta[i], 2) - 2 * (1 - profiles->eta[i]) * (pow(profiles->eta[i], 2) - 3 * pow(profiles->eta[i], 3) + 3 * pow(profiles->eta[i], 4) - pow(profiles->eta[i], 5));

            profiles->dU_deta[i] = -1.8 * A * (A - 3) * pow(profiles->eta[i], 2) * (profiles->eta[i] - 3 * pow(profiles->eta[i], 2) + 3 * pow(profiles->eta[i], 3) - pow(profiles->eta[i], 4)) + A * (1 - 0.6 * (A - 3) * pow(profiles->eta[i], 3)) * f1 + f0;
            profiles->dW_deta[i] = B * f2 + Psi * f3;

            mu_mui = (1 / profiles->R[i], 1.5) * 2 / (1 / profiles->R[i] + 1);

            profiles->S[i] = (mu_mui / Re_delta) * profiles->dU_deta[i];
            profiles->T[i] = (mu_mui / Re_delta) * profiles->dW_deta[i];
        }
    }
    else
    {

        // u_plus_max
        if (delta_plus <= y_min)
        {
            u_plus_max = delta_plus;
        }
        else if ((y_min < delta_plus) && (delta_plus < y_max))
        {
            t = (log10(delta_plus) - log_y_min) / (log_y_max - log_y_min);
            u_plus_max = pow(1 - t, 4) * u_min + 4 * pow(1 - t, 3) * t * a + 6 * pow(1 - t, 2) * pow(t, 2) * b + 4 * (1 - t) * pow(t, 3) * c + pow(t, 4) * u_max;
        }
        else
        {
            u_plus_max = (1 / k) * log(delta_plus) + C;
        }

        K = sqrt(pow(Wtau * u_plus_max, 2) + pow(1 - Utau * u_plus_max, 2));
        Upsilon = atan(Wtau * u_plus_max / (1 - Utau * u_plus_max));

        // Velocities
        for (i = 0; i < profiles->n; i++)
        {

            // u_plus
            if (y_plus[i] < y_min)
            {
                u_plus[i] = y_plus[i];
            }
            else if ((y_min <= y_plus[i]) && (y_plus[i] <= y_max))
            {
                t = (log10(y_plus[i]) - log_y_min) / (log_y_max - log_y_min);
                u_plus[i] = pow(1 - t, 4) * u_min + 4 * pow(1 - t, 3) * t * a + 6 * pow(1 - t, 2) * pow(t, 2) * b + 4 * (1 - t) * pow(t, 3) * c + pow(t, 4) * u_max;
            }
            else
            {
                u_plus[i] = (1 / k) * log(y_plus[i]) + C;
            }

            g0 = 3 * pow(profiles->eta[i], 2) - 2 * pow(profiles->eta[i], 3);

            profiles->U[i] = Utau * u_plus[i] + K * cos(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * g0;
            profiles->W[i] = Wtau * u_plus[i] - K * sin(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * g0;

            profiles->R[i] = 1 / (1 + epsilon_line * (1 - pow(profiles->U[i], 2) - pow(profiles->W[i], 2)));
        }

        // Gradient and Shear stress
        for (i = 0; i < profiles->n; i++)
        {

            g0 = 3 * pow(profiles->eta[i], 2) - 2 * pow(profiles->eta[i], 3);

            profiles->dU_deta[i] = Utau * delta_plus * (1 / (1 + exp(-k * C) * (k * exp(k * u_plus[i]) - k - pow(k, 2) * u_plus[i] - 0.5 * k * pow(k * u_plus[i], 2)))) + 2 * Psi * (1 - profiles->eta[i]) * K * sin(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * g0 + K * cos(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * (6 * profiles->eta[i] - 6 * pow(profiles->eta[i], 2));
            profiles->dW_deta[i] = Wtau * delta_plus * (1 / (1 + exp(-k * C) * (k * exp(k * u_plus[i]) - k - pow(k, 2) * u_plus[i] - 0.5 * k * pow(k * u_plus[i], 2)))) + 2 * Psi * (1 - profiles->eta[i]) * K * cos(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * g0 - K * sin(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * (6 * profiles->eta[i] - 6 * pow(profiles->eta[i], 2));

            mu_mui = (1 / profiles->R[i], 1.5) * 2 / (1 / profiles->R[i] + 1);

            g0 = 3 * pow(profiles->eta[i], 2) - 2 * pow(profiles->eta[i], 3);
            dg0deta = 6 * profiles->eta[i] - 6 * pow(profiles->eta[i], 2);

            profiles->S[i] = profiles->R[i] * Utau * sqrt(pow(Utau, 2) + pow(Wtau, 2)) * (1 - g0) + profiles->R[i] * Ctau1 * K * cos(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * dg0deta;
            profiles->T[i] = profiles->R[i] * Wtau * sqrt(pow(Utau, 2) + pow(Wtau, 2)) * (1 - g0) + profiles->R[i] * Ctau2 * K * sin(Upsilon - Psi * pow(1 - profiles->eta[i], 2)) * dg0deta;
        }
    }

    /* Free arrays */
    free(u_plus);
    free(y_plus);
}

void calculateIntegralThickness(struct ProfileParameters *profiles,
                                struct IntegralThicknessParameters *integralThickness,
                                double delta,
                                double Psi)
{

    /* Parameters */
    int i;
    double *func;

    /* Initialize */
    func = (double *)malloc(profiles->n * sizeof(double));

    /* Calculate integral */
    for (i = 0; i < profiles->n; i++)
        func[i] = 1 - profiles->R[i] * profiles->U[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->delta_1_ast, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -profiles->R[i] * profiles->W[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->delta_2_ast, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = 1 - profiles->R[i] * pow(profiles->U[i], 2);
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->phi_11, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -profiles->R[i] * profiles->U[i] * profiles->W[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->phi_12, delta);
    integralThickness->phi_21 = integralThickness->phi_12;

    for (i = 0; i < profiles->n; i++)
        func[i] = -profiles->R[i] * pow(profiles->W[i], 2);
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->phi_22, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = 1 - profiles->R[i] * profiles->U[i] * (pow(profiles->U[i], 2) + pow(profiles->W[i], 2));
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->phi_1_ast, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -profiles->R[i] * profiles->W[i] * (pow(profiles->U[i], 2) + pow(profiles->W[i], 2));
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->phi_2_ast, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = 1 - profiles->U[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->delta_1_line, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -profiles->W[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->delta_2_line, delta);

    integralThickness->delta_q = integralThickness->phi_11 + integralThickness->phi_22;

    for (i = 0; i < profiles->n; i++)
        func[i] = -Psi * profiles->R[i] * (pow(profiles->U[i], 2) + pow(profiles->W[i], 2));
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->delta_q_o, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -Psi * profiles->R[i] * profiles->U[i] * (pow(profiles->U[i], 2) + pow(profiles->W[i], 2));
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->theta_1_o, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -Psi * profiles->R[i] * profiles->W[i] * (pow(profiles->U[i], 2) + pow(profiles->W[i], 2));
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->theta_2_o, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -Psi * profiles->U[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->delta_1_o, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = -Psi * profiles->W[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->delta_2_o, delta);

    for (i = 0; i < profiles->n; i++)
        func[i] = profiles->S[i] * profiles->dU_deta[i] + profiles->T[i] * profiles->dW_deta[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->C_D, 1.0);

    for (i = 0; i < profiles->n; i++)
        func[i] = profiles->S[i] * profiles->dW_deta[i] - profiles->T[i] * profiles->dU_deta[i];
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->C_D_x, 1.0);

    for (i = 0; i < profiles->n; i++)
        func[i] = Psi * (profiles->S[i] * profiles->dW_deta[i] - profiles->T[i] * profiles->dU_deta[i]);
    integrate_trap(profiles->n, profiles->eta, func, &integralThickness->C_D_o, 1.0);

    integralThickness->C_f_1 = 2 * profiles->S[0];
    integralThickness->C_f_2 = 2 * profiles->T[0];

    integralThickness->theta_11 = integralThickness->phi_11 - integralThickness->delta_1_line;
    integralThickness->theta_22 = integralThickness->phi_22 - integralThickness->delta_2_line;

    /* Free arrays */
    free(func);
}

void calculateIntegralDefect(struct ProfileParameters *profiles,
                             struct IntegralThicknessParameters *integralThickness,
                             struct FreestreamParameters *freestream,
                             struct IntegralDefectParameters *integralDefect,
                             double delta,
                             double A, double B,
                             double Ctau1, double Ctau2) {

    /* Aux */
    double aux_1, aux_2, aux_3;

    /* Initialize */
    aux_1 = freestream->density * freestream->velocity;
    aux_2 = freestream->density * freestream->velocity * freestream->velocity;
    aux_3 = freestream->density * freestream->velocity * freestream->velocity * freestream->velocity;

    /* Calculate defect parameters */
    integralDefect->M_x = aux_1 * integralThickness->delta_1_ast;
    integralDefect->M_y = aux_1 * integralThickness->delta_2_ast;

    integralDefect->J_xx = aux_2 * integralThickness->phi_11;
    integralDefect->J_xy = aux_2 * integralThickness->phi_12;
    integralDefect->J_yx = aux_2 * integralThickness->phi_21;
    integralDefect->J_yy = aux_2 * integralThickness->phi_22;

    integralDefect->E_x = aux_3 * integralThickness->phi_1_ast;
    integralDefect->E_y = aux_3 * integralThickness->phi_2_ast;

    integralDefect->K_o_x = aux_3 * integralThickness->theta_1_o;
    integralDefect->K_o_y = aux_3 * integralThickness->theta_2_o;

    integralDefect->Q_x = freestream->velocity * integralThickness->delta_1_line;
    integralDefect->Q_y = freestream->velocity * integralThickness->delta_2_line;

    integralDefect->Q_o_x = freestream->velocity * integralThickness->theta_1_o;
    integralDefect->Q_o_y = freestream->velocity * integralThickness->theta_2_o;

    integralDefect->tau_w_x = 0.5 * aux_2 * integralThickness->C_f_1;
    integralDefect->tau_w_y = 0.5 * aux_2 * integralThickness->C_f_2;

    integralDefect->D = aux_3 * integralThickness->C_D;
    integralDefect->D_x = aux_3 * integralThickness->C_D_x;
    integralDefect->D_o = aux_3 * integralThickness->C_D_o;

    double mod_Ctau = sqrt(pow(Ctau1, 2) + pow(Ctau2, 2));

    if (mod_Ctau <= CTAU_CRIT) {

        double H_k_1, H_k_2, Re_theta_11, Re_theta_22, f1, f2;

        H_k_1 = (integralThickness->delta_1_ast / integralThickness->theta_11 - 0.29 * freestream->mach * freestream->mach) / (1 + 0.113 * freestream->mach * freestream->mach);
        Re_theta_11 = freestream->velocity * freestream->density * integralThickness->theta_11 / freestream->viscosity;
        f1 = 0.01 * sqrt(pow(2.4 * H_k_1 - 3.7 + 2.5 * tanh(1.5 * H_k_1 - 4.65), 2) + 0.25) * (Re_theta_11 - pow(10, (1.415 / (H_k_1 - 1) - 0.489) * tanh(20 / (H_k_1 - 1) - 12.9)));
        integralDefect->S_tau_x = f1 * freestream->velocity * mod_Ctau / integralThickness->theta_11;

        if (absValue(integralThickness->theta_22) < 1e-10) {
            integralDefect->S_tau_y = 0.0;
        } else {
            H_k_2 = (integralThickness->delta_2_ast / integralThickness->theta_22 - 0.29 * freestream->mach * freestream->mach) / (1 + 0.113 * freestream->mach * freestream->mach);
            Re_theta_22 = freestream->velocity * freestream->density * integralThickness->theta_22 / freestream->viscosity;
            f2 = 0.01 * sqrt(pow(2.4 * H_k_2 - 3.7 + 2.5 * tanh(1.5 * H_k_2 - 4.65), 2) + 0.25) * (Re_theta_22 - pow(10, (1.415 / (H_k_2 - 1) - 0.489) * tanh(20 / (H_k_2 - 1) - 12.9)));
            integralDefect->S_tau_y = f2 * freestream->velocity * mod_Ctau / integralThickness->theta_22;
        }

    } else {

        double P_tau_x;
        double P_tau_y;
        double D_tau_x;
        double D_tau_y;

        double *P_tau_x_func = (double *)malloc(profiles->n * sizeof(double));
        double *P_tau_y_func = (double *)malloc(profiles->n * sizeof(double));
        double *D_tau_x_func = (double *)malloc(profiles->n * sizeof(double));
        double *D_tau_y_func = (double *)malloc(profiles->n * sizeof(double));

        double tau_x, tau_y;

        for (int i = 0; i < profiles->n; i++) {

            P_tau_x_func[i] = freestream->density * profiles->R[i] * (freestream->velocity * freestream->velocity / profiles->R[i]) * sqrt(pow(profiles->S[i], 2) + pow(profiles->T[i], 2)) * freestream->velocity * profiles->dU_deta[i];
            P_tau_y_func[i] = freestream->density * profiles->R[i] * (freestream->velocity * freestream->velocity / profiles->R[i]) * sqrt(pow(profiles->S[i], 2) + pow(profiles->T[i], 2)) * freestream->velocity * profiles->dW_deta[i];

            tau_x = freestream->velocity * freestream->velocity * profiles->S[i] / profiles->R[i];
            tau_y = freestream->velocity * freestream->velocity * profiles->T[i] / profiles->R[i];

            D_tau_x_func[i] = 2 * freestream->density * profiles->R[i] * pow(pow(tau_x, 2) + pow(tau_y, 2), 0.25) * tau_x;
            D_tau_y_func[i] = 2 * freestream->density * profiles->R[i] * pow(pow(tau_x, 2) + pow(tau_y, 2), 0.25) * tau_y;
        }

        integrate_trap(profiles->n, profiles->eta, P_tau_x_func, &P_tau_x, 1.0);
        integrate_trap(profiles->n, profiles->eta, P_tau_y_func, &P_tau_y, 1.0);
        integrate_trap(profiles->n, profiles->eta, D_tau_x_func, &D_tau_x, delta);
        integrate_trap(profiles->n, profiles->eta, D_tau_y_func, &D_tau_y, delta);

        integralDefect->S_tau_x = 0.30 * (P_tau_x - D_tau_x);
        integralDefect->S_tau_y = 0.30 * (P_tau_y - D_tau_y);

        free(P_tau_x_func);
        free(P_tau_y_func);
        free(D_tau_x_func);
        free(D_tau_y_func);

    }

    double *K_tau_xx_func = (double *)malloc(profiles->n * sizeof(double));
    double *K_tau_xy_func = (double *)malloc(profiles->n * sizeof(double));
    double *K_tau_yx_func = (double *)malloc(profiles->n * sizeof(double));
    double *K_tau_yy_func = (double *)malloc(profiles->n * sizeof(double));

    double tau_x, tau_y;

    for (int i = 0; i < profiles->n; i++) {

        tau_x = freestream->velocity * freestream->velocity * profiles->S[i] / profiles->R[i];
        tau_y = freestream->velocity * freestream->velocity * profiles->T[i] / profiles->R[i];

        K_tau_xx_func[i] = profiles->R[i] * freestream->density * tau_x * freestream->velocity * profiles->U[i];
        K_tau_xy_func[i] = profiles->R[i] * freestream->density * tau_x * freestream->velocity * profiles->W[i];
        K_tau_yx_func[i] = profiles->R[i] * freestream->density * tau_y * freestream->velocity * profiles->U[i];
        K_tau_yy_func[i] = profiles->R[i] * freestream->density * tau_y * freestream->velocity * profiles->W[i];
    }

    integrate_trap(profiles->n, profiles->eta, K_tau_xx_func, &integralDefect->K_tau_xx, delta);
    integrate_trap(profiles->n, profiles->eta, K_tau_xy_func, &integralDefect->K_tau_xy, delta);
    integrate_trap(profiles->n, profiles->eta, K_tau_yx_func, &integralDefect->K_tau_yx, delta);
    integrate_trap(profiles->n, profiles->eta, K_tau_yy_func, &integralDefect->K_tau_yy, delta);

    free(K_tau_xx_func);
    free(K_tau_xy_func);
    free(K_tau_yx_func);
    free(K_tau_yy_func);
}

void calculateEquationsParams(double delta,
                              double A,
                              double B,
                              double Psi,
                              double Ctau1,
                              double Ctau2,
                              struct FreestreamParameters *freestream,
                              struct ProfileParameters *profiles,
                              struct IntegralThicknessParameters *integralThickness,
                              struct IntegralDefectParameters *integralDefect,
                              struct EquationsParameters *params)
{

    /* Profiles */
    calculateProfiles(delta, A, B, Psi, Ctau1, Ctau2, freestream, profiles);

    /* Integral thickness */
    calculateIntegralThickness(profiles, integralThickness, delta, Psi);

    /* Integral defect */
    calculateIntegralDefect(profiles, integralThickness, freestream, integralDefect, delta, A, B, Ctau1, Ctau2);

    /* Assing params */
    params->D = integralDefect->D;
    params->D_o = integralDefect->D_o;
    params->D_x = integralDefect->D_x;
    params->E_x = integralDefect->E_x;
    params->E_y = integralDefect->E_y;
    params->J_xx = integralDefect->J_xx;
    params->J_xy = integralDefect->J_xy;
    params->J_yx = integralDefect->J_yx;
    params->J_yy = integralDefect->J_yy;
    params->K_o_x = integralDefect->K_o_x;
    params->K_o_y = integralDefect->K_o_y;
    params->K_tau_xx = integralDefect->K_tau_xx;
    params->K_tau_xy = integralDefect->K_tau_xy;
    params->K_tau_yx = integralDefect->K_tau_yx;
    params->K_tau_yy = integralDefect->K_tau_yy;
    params->M_x = integralDefect->M_x;
    params->M_y = integralDefect->M_y;
    params->Q_o_x = integralDefect->Q_o_x;
    params->Q_o_y = integralDefect->Q_o_y;
    params->Q_x = integralDefect->Q_x;
    params->Q_y = integralDefect->Q_y;
    params->S_tau_x = integralDefect->S_tau_x;
    params->S_tau_y = integralDefect->S_tau_y;
    params->tau_w_x = integralDefect->tau_w_x;
    params->tau_w_y = integralDefect->tau_w_y;
    params->vel = freestream->velocity;
    params->density = freestream->density;
}

void calculateDivergents(int face,
                         int *faces,
                         struct VerticeConnection *vertices_connection,
                         struct EquationsParameters *params,
                         double area,
                         double *p1, double *p2, double *p3) {

    /* Parameters */
    int k; // Counters

    struct Point point_1, point_2, point_3; // Face corners
    struct Point v_1, v_2, v_3;             // Face corners values
    struct Point point_lateral;             // Face edge orthogonal vector
    int index_1, index_2, index_3;          // Vertices indexes

    /* Initialize */
    point_1.x = p1[face];
    point_1.y = p1[face + 1];
    point_1.z = 0.0;
    point_2.x = p2[face];
    point_2.y = p2[face + 1];
    point_2.z = 0.0;
    point_3.x = p3[face];
    point_3.y = p3[face + 1];
    point_3.z = 0.0;

    v_1.z = 0.0;
    v_2.z = 0.0;
    v_3.z = 0.0;

    index_1 = faces[3 * face];
    index_2 = faces[3 * face + 1];
    index_3 = faces[3 * face + 2];

    /* Div(M) */
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].M_x;
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].M_y;
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].M_x;
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].M_y;
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].M_x;
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].M_y;
    }

    calculateDivergence(point_1, point_2, point_3, v_1, v_2, v_3, area, &params[face].div_M);

    /* Div(Jx) */
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].J_xx;
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].J_xy;
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].J_xx;
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].J_xy;
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].J_xx;
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].J_xy;
    }

    calculateDivergence(point_1, point_2, point_3, v_1, v_2, v_3, area, &params[face].div_J_x);

    /* Div(Jy) */
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].J_yx;
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].J_yy;
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].J_yx;
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].J_yy;
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].J_yx;
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].J_yy;
    }

    calculateDivergence(point_1, point_2, point_3, v_1, v_2, v_3, area, &params[face].div_J_y);

    /* Div(E) */
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].E_x;
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].E_y;
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].E_x;
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].E_y;
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].E_x;
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].E_y;
    }

    calculateDivergence(point_1, point_2, point_3, v_1, v_2, v_3, area, &params[face].div_E);

    /* Div(ko) */
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].K_o_x;
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].K_o_y;
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].K_o_x;
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].K_o_y;
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].K_o_x;
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].K_o_y;
    }

    calculateDivergence(point_1, point_2, point_3, v_1, v_2, v_3, area, &params[face].div_K_o);

    /* Div(ktaux) */
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].K_tau_xx;
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].K_tau_xy;
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].K_tau_xx;
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].K_tau_xy;
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].K_tau_xx;
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].K_tau_xy;
    }

    calculateDivergence(point_1, point_2, point_3, v_1, v_2, v_3, area, &params[face].div_K_tau_x);

    /* Div(ktauy) */
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].K_tau_yx;
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * params[vertices_connection[index_1].faces[k]].K_tau_yy;
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].K_tau_yx;
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * params[vertices_connection[index_2].faces[k]].K_tau_yy;
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].K_tau_yx;
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * params[vertices_connection[index_3].faces[k]].K_tau_yy;
    }

    calculateDivergence(point_1, point_2, point_3, v_1, v_2, v_3, area, &params[face].div_K_tau_y);
}

void calculateGradients(int face,
                        int *faces,
                        struct VerticeConnection *vertices_connection,
                        struct EquationsParameters *params,
                        double *e1, double *e2, double *e3,
                        double *p1, double *p2, double *p3,
                        double *velNorm,
                        double *velx, double *vely, double *velz,
                        double *transpiration)
{

    /* Parameters */
    int k; // Counters

    struct Point point_1, point_2, point_3;    // Face corners
    struct Point v_1, v_2, v_3;                // Face corners values
    struct Point e1_point, e2_point, e3_point; // Face base vectors
    struct Point vel_point;                    // Face velocity
    struct Point point_lateral;                // Face edge orthogonal vector
    int index_1, index_2, index_3;             // Vertices indexes
    double aux_1, aux_2;

    /* Initialize */
    e1_point.x = e1[3 * face];
    e1_point.y = e1[3 * face + 1];
    e1_point.z = e1[3 * face + 2];
    e2_point.x = e2[3 * face];
    e2_point.y = e2[3 * face + 1];
    e2_point.z = e2[3 * face + 2];
    e3_point.x = e3[3 * face];
    e3_point.y = e3[3 * face + 1];
    e3_point.z = e3[3 * face + 2];

    point_1.x = p1[2 * face];
    point_1.y = p1[2 * face + 1];
    point_1.z = 0.0;
    point_2.x = p2[2 * face];
    point_2.y = p2[2 * face + 1];
    point_2.z = 0.0;
    point_3.x = p3[2 * face];
    point_3.y = p3[2 * face + 1];
    point_3.z = 0.0;

    v_1.z = 0.0;
    v_2.z = 0.0;
    v_3.z = 0.0;

    index_1 = faces[3 * face];
    index_2 = faces[3 * face + 1];
    index_3 = faces[3 * face + 2];

    vel_point.x = velx[face];
    vel_point.y = vely[face];
    vel_point.z = velz[face];

    // pow(velNorm, 2)
    point_1.z = 0.0;
    point_2.z = 0.0;
    point_3.z = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        point_1.z = point_1.z + vertices_connection[index_1].coeffs[k] * velNorm[vertices_connection[index_1].faces[k]] * velNorm[vertices_connection[index_1].faces[k]];
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        point_2.z = point_2.z + vertices_connection[index_2].coeffs[k] * velNorm[vertices_connection[index_2].faces[k]] * velNorm[vertices_connection[index_2].faces[k]];
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        point_3.z = point_3.z + vertices_connection[index_3].coeffs[k] * velNorm[vertices_connection[index_3].faces[k]] * velNorm[vertices_connection[index_3].faces[k]];
    }

    calculateGradient(velNorm[face] * velNorm[face], point_1, point_2, point_3, e1_point, e2_point, e3_point, vel_point, transpiration[face], &params[face].grad_q2_x, &params[face].grad_q2_y);

    // Phi
    v_1.x = 0.0;
    v_1.y = 0.0;
    v_1.z = 0.0;
    v_2.x = 0.0;
    v_2.y = 0.0;
    v_2.z = 0.0;
    v_3.x = 0.0;
    v_3.y = 0.0;
    v_3.z = 0.0;

    for (k = 0; k < vertices_connection[index_1].n; k++)
    {
        v_1.x = v_1.x + vertices_connection[index_1].coeffs[k] * velx[vertices_connection[index_1].faces[k]];
        v_1.y = v_1.y + vertices_connection[index_1].coeffs[k] * vely[vertices_connection[index_1].faces[k]];
        v_1.z = v_1.z + vertices_connection[index_1].coeffs[k] * velz[vertices_connection[index_1].faces[k]];
    }

    for (k = 0; k < vertices_connection[index_2].n; k++)
    {
        v_2.x = v_2.x + vertices_connection[index_2].coeffs[k] * velx[vertices_connection[index_2].faces[k]];
        v_2.y = v_2.y + vertices_connection[index_2].coeffs[k] * vely[vertices_connection[index_2].faces[k]];
        v_2.z = v_2.z + vertices_connection[index_2].coeffs[k] * velz[vertices_connection[index_2].faces[k]];
    }

    for (k = 0; k < vertices_connection[index_3].n; k++)
    {
        v_3.x = v_3.x + vertices_connection[index_3].coeffs[k] * velx[vertices_connection[index_3].faces[k]];
        v_3.y = v_3.y + vertices_connection[index_3].coeffs[k] * vely[vertices_connection[index_3].faces[k]];
        v_3.z = v_3.z + vertices_connection[index_3].coeffs[k] * velz[vertices_connection[index_3].faces[k]];
    }

    // Remove vel in e3 direction
    aux_1 = e3_point.x * vel_point.x + e3_point.y * vel_point.y + e3_point.z * vel_point.z;
    vel_point.x = vel_point.x - e3_point.x * aux_1;
    vel_point.y = vel_point.y - e3_point.y * aux_1;
    vel_point.z = vel_point.z - e3_point.z * aux_1;

    aux_1 = e3_point.x * v_1.x + e3_point.y * v_1.y + e3_point.z * v_1.z;
    v_1.x = v_1.x - e3_point.x * aux_1;
    v_1.y = v_1.y - e3_point.y * aux_1;
    v_1.z = v_1.z - e3_point.z * aux_1;

    aux_1 = e3_point.x * v_2.x + e3_point.y * v_2.y + e3_point.z * v_2.z;
    v_2.x = v_2.x - e3_point.x * aux_1;
    v_2.y = v_2.y - e3_point.y * aux_1;
    v_2.z = v_2.z - e3_point.z * aux_1;

    aux_1 = e3_point.x * v_3.x + e3_point.y * v_3.y + e3_point.z * v_3.z;
    v_3.x = v_3.x - e3_point.x * aux_1;
    v_3.y = v_3.y - e3_point.y * aux_1;
    v_3.z = v_3.z - e3_point.z * aux_1;

    // Unary vectors
    aux_1 = norm(vel_point);
    vel_point.x = vel_point.x / aux_1;
    vel_point.y = vel_point.y / aux_1;
    vel_point.z = vel_point.z / aux_1;
    aux_1 = norm(v_1);
    v_1.x = v_1.x / aux_1;
    v_1.y = v_1.y / aux_1;
    v_1.z = v_1.z / aux_1;
    aux_1 = norm(v_2);
    v_2.x = v_2.x / aux_1;
    v_2.y = v_2.y / aux_1;
    v_2.z = v_2.z / aux_1;
    aux_1 = norm(v_3);
    v_3.x = v_3.x / aux_1;
    v_3.y = v_3.y / aux_1;
    v_3.z = v_3.z / aux_1;

    point_lateral = cross(vel_point, e3_point);

    aux_1 = point_lateral.x * v_1.x + point_lateral.y * v_1.y + point_lateral.z * v_1.z;
    aux_2 = vel_point.x * v_1.x + vel_point.y * v_1.y + vel_point.z * v_1.z;
    point_1.z = atan(aux_1 / aux_2);

    aux_1 = point_lateral.x * v_2.x + point_lateral.y * v_2.y + point_lateral.z * v_2.z;
    aux_2 = vel_point.x * v_2.x + vel_point.y * v_2.y + vel_point.z * v_2.z;
    point_2.z = atan(aux_1 / aux_2);

    aux_1 = point_lateral.x * v_3.x + point_lateral.y * v_3.y + point_lateral.z * v_3.z;
    aux_2 = vel_point.x * v_3.x + vel_point.y * v_3.y + vel_point.z * v_3.z;
    point_3.z = atan(aux_1 / aux_2);

    vel_point.x = velx[face];
    vel_point.y = vely[face];
    vel_point.z = velz[face];

    calculateGradient(0.0, point_1, point_2, point_3, e1_point, e2_point, e3_point, vel_point, transpiration[face], &params[face].grad_phi_x, &params[face].grad_phi_y);
}

void calculateObjectiveFunction(struct EquationsParameters params,
                                double *momentum_x,
                                double *momentum_y,
                                double *kinetic_energy,
                                double *lateral_curvature,
                                double *shear_stress_x,
                                double *shear_stress_y,
                                double velocity) {

    /* Face integral equations */
    *momentum_x = (params.div_J_x - params.vel * params.div_M - params.tau_w_x) / (params.density * params.vel * params.vel);
    *momentum_y = (params.div_J_y - params.tau_w_y) / (params.density * params.vel * params.vel);
    *kinetic_energy = (params.div_E - params.vel * params.vel * params.div_M - params.density * (params.Q_x * params.grad_q2_x + params.Q_y * params.grad_q2_y) - 2 * params.D) / (params.density * params.vel * params.vel * params.vel);
    *lateral_curvature = (params.div_K_o + (params.E_x * params.grad_phi_x + params.E_y * params.grad_phi_y) + 0.5 * params.density * (params.Q_x * params.grad_q2_y - params.Q_y * params.grad_q2_x) - params.density * (params.Q_o_x * params.grad_q2_x + params.Q_o_y * params.grad_q2_y) + params.D_x - 2 * params.D_o) / (params.density * params.vel * params.vel * params.vel);
    *shear_stress_x = (params.div_K_tau_x - params.S_tau_x) / (params.density * params.vel * params.vel);
    *shear_stress_y = params.div_K_tau_y - params.S_tau_y / (params.density * params.vel * params.vel);
}

void addSparseValue(double *a, int *ia, int *ja, int *index, double value, int row, int col) {
    a[*index] = value;
    ia[*index] = row;
    ja[*index] = col;
    *index = *index + 1;
}

void solveBoundaryLayer(int nf,
                        int nv,
                        struct VerticeConnection *vertices_connection,
                        double *vertices,
                        int *faces,
                        double *facesCenter,
                        double *facesArea,
                        double *e1, double *e2, double *e3,
                        double *p1, double *p2, double *p3,
                        double *transpiration,
                        double *delta,
                        double *A,
                        double *B,
                        double *Psi,
                        double *Ctau1,
                        double *Ctau2,
                        double *tau_x,
                        double *tau_y,
                        double *velNorm,
                        double *velx, double *vely, double *velz,
                        double *mach,
                        double density,
                        double viscosity,
                        double *cp,
                        double sound_speed,
                        double *matrix, double *array,
                        double *matrixVelx, double *matrixVely, double *matrixVelz, double *arrayVel,
                        double *doublet,
                        double freestreamNorm) {

    /* Parameters */
    int i, j, k, l; // loop variables
    int int_max; // maximum interaction

    struct EquationsParameters params_aux;        // aux
    struct EquationsParameters *params;           // equations parameters
    struct EquationsParameters *params_delta_eps; // equations parameters eith and delta + eps
    struct EquationsParameters *params_A_eps;     // equations parameters eith and A + eps
    struct EquationsParameters *params_B_eps;     // equations parameters eith and B + eps
    struct EquationsParameters *params_Psi_eps;   // equations parameters eith and Psi + eps
    struct EquationsParameters *params_Ctau1_eps; // equations parameters eith and Ctau1 + eps
    struct EquationsParameters *params_Ctau2_eps; // equations parameters eith and Ctau2 + eps

    double *norm_delta_list; // normalized delta
    double *norm_A_list;     // normalized A
    double *norm_B_list;     // normalized B
    double *norm_Psi_list;   // normalized Psi
    double *norm_Ctau1_list; // normalized Ctau1
    double *norm_Ctau2_list; // normalized Ctau2
    double norm_delta;       // value to normalize delta
    double norm_A;           // value to normalize A
    double norm_B;           // value to normalize B
    double norm_Psi;         // value to normalize Psi
    double norm_Ctau1;       // value to normalize Ctau1
    double norm_Ctau2;       // value to normalize Ctau2
    double eps;              // small value to calculate derivatives

    struct FreestreamParameters freestream;               // freestream parameters
    struct ProfileParameters profiles;                    // face profiles
    struct IntegralThicknessParameters integralThickness; // integral thickness paramters
    struct IntegralDefectParameters integralDefect;       // integral defect parameters

    struct FacesConnection *faces_connection; // faces connection

    double max_error;      // Maximum error
    double max_momentum_x, max_momentum_x_aux;
    double max_momentum_y, max_momentum_y_aux;
    double max_kinetic_energy, max_kinetic_energy_aux;
    double max_lateral_curvature, max_lateral_curvature_aux;
    double max_shear_stress_x, max_shear_stress_x_aux;
    double max_shear_stress_y, max_shear_stress_y_aux;

    /* Initialize */
    int_max = 500;

    params = (struct EquationsParameters *)malloc(nf * sizeof(struct EquationsParameters));
    params_delta_eps = (struct EquationsParameters *)malloc(nf * sizeof(struct EquationsParameters));
    params_A_eps = (struct EquationsParameters *)malloc(nf * sizeof(struct EquationsParameters));
    params_B_eps = (struct EquationsParameters *)malloc(nf * sizeof(struct EquationsParameters));
    params_Psi_eps = (struct EquationsParameters *)malloc(nf * sizeof(struct EquationsParameters));
    params_Ctau1_eps = (struct EquationsParameters *)malloc(nf * sizeof(struct EquationsParameters));
    params_Ctau2_eps = (struct EquationsParameters *)malloc(nf * sizeof(struct EquationsParameters));

    norm_delta_list = (double *)malloc(nf * sizeof(double));
    norm_A_list = (double *)malloc(nf * sizeof(double));
    norm_B_list = (double *)malloc(nf * sizeof(double));
    norm_Psi_list = (double *)malloc(nf * sizeof(double));
    norm_Ctau1_list = (double *)malloc(nf * sizeof(double));
    norm_Ctau2_list = (double *)malloc(nf * sizeof(double));

    double max_x_value = -10.0;
    for (i = 0; i < nv; i++) if (vertices[3 * i] > max_x_value) max_x_value = vertices[3 * i];

    for (i = 0; i < nf; i++) {
        norm_delta_list[i] = (1 / 1e-3) * (0.001 + 5.0 * (max_x_value - facesCenter[3 * i]) / sqrt(density * freestreamNorm * (max_x_value - facesCenter[3 * i]) / viscosity));
        norm_A_list[i] = 1.0;
        norm_B_list[i] = 0.001;
        norm_Psi_list[i] = 0.001;
        norm_Ctau1_list[i] = 0.001;
        norm_Ctau2_list[i] = 0.001;
    }

    norm_delta = 1e-3;
    norm_A = 1;
    norm_B = 1;
    norm_Psi = 1;
    norm_Ctau1 = 1e-4;
    norm_Ctau2 = 1e-4;
    eps = 1e-8;

    freestream.density = density;
    freestream.viscosity = viscosity;

    profiles.n = LAYERS;
    profiles.eta = (double *)malloc(nf * sizeof(double));
    profiles.U = (double *)malloc(nf * sizeof(double));
    profiles.W = (double *)malloc(nf * sizeof(double));
    profiles.S = (double *)malloc(nf * sizeof(double));
    profiles.T = (double *)malloc(nf * sizeof(double));
    profiles.R = (double *)malloc(nf * sizeof(double));
    profiles.dU_deta = (double *)malloc(nf * sizeof(double));
    profiles.dW_deta = (double *)malloc(nf * sizeof(double));

    faces_connection = (struct FacesConnection *)malloc(nf * sizeof(struct FacesConnection));

    double *obj_func_comp = (double *)malloc(1800 * sizeof(double));
    double *grad_momentum_x = (double *)malloc(300 * sizeof(double));
    double *grad_momentum_y = (double *)malloc(300 * sizeof(double));
    double *grad_kinetic_energy = (double *)malloc(300 * sizeof(double));
    double *grad_lateral_curvature = (double *)malloc(300 * sizeof(double));
    double *grad_shear_stress_x = (double *)malloc(300 * sizeof(double));
    double *grad_shear_stress_y = (double *)malloc(300 * sizeof(double));

    double max_sparse_array;

    /* Faces connection */
    calculateFacesConnection(nv, nf, faces, vertices_connection, faces_connection);

    double *sparse_a;
    int *sparse_ia;
    int *sparse_ja;
    double *sparse_array;
    int index_sparse;

    int size_sparse_a = 0;

    for (i = 0; i < nf; i++) size_sparse_a = size_sparse_a + 1 + faces_connection[i].n;
    size_sparse_a = size_sparse_a * 36;

    sparse_a = (double *)malloc(size_sparse_a * sizeof(double));
    sparse_ia = (int *)malloc(size_sparse_a * sizeof(int));
    sparse_ja = (int *)malloc(size_sparse_a * sizeof(int));
    sparse_array = (double *)malloc(nf * 6 * sizeof(double));
    double *increase = (double *)malloc(nf * 6 * sizeof(double));

    /* Print Interactions */
    printf("\n      Interaction   Momentum x       Momentum y    Kinetic Energy    Lateral Curv.   Shear Stress x   Shear Stress y\n");

    /* Interaction loop */
    for (i = 0; i < int_max; i++) {

        /* Calculate integrals defect of all faces */
        for (j = 0; j < nf; j++) {

            freestream.velocity = velNorm[j];
            freestream.mach = mach[j];

            calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * norm_A_list[j], norm_B * norm_B_list[j], norm_Psi * norm_Psi_list[j], norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params[j]);
            calculateEquationsParams(norm_delta * (norm_delta_list[j] + eps), norm_A * norm_A_list[j], norm_B * norm_B_list[j], norm_Psi * norm_Psi_list[j], norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params_delta_eps[j]);
            calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * (norm_A_list[j] + eps), norm_B * norm_B_list[j], norm_Psi * norm_Psi_list[j], norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params_A_eps[j]);
            calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * norm_A_list[j], norm_B * (norm_B_list[j] + eps), norm_Psi * norm_Psi_list[j], norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params_B_eps[j]);
            calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * norm_A_list[j], norm_B * norm_B_list[j], norm_Psi * (norm_Psi_list[j] + eps), norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params_Psi_eps[j]);
            calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * norm_A_list[j], norm_B * norm_B_list[j], norm_Psi * norm_Psi_list[j], norm_Ctau1 * (norm_Ctau1_list[j] + eps), norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params_Ctau1_eps[j]);
            calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * norm_A_list[j], norm_B * norm_B_list[j], norm_Psi * norm_Psi_list[j], norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * (norm_Ctau2_list[j] + eps), &freestream, &profiles, &integralThickness, &integralDefect, &params_Ctau2_eps[j]);
        }

        /* Initial system index */
        index_sparse = 0;

        /* Faces loop */
        for (j = 0; j < nf; j++) {

            /* Calculate reference value */
            calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

            /* Surface shear stress */
            tau_x[j] = params[j].tau_w_x;
            tau_y[j] = params[j].tau_w_y;

            /* Ref. obj. func. */
            calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

            sparse_array[6 * j] = - max_momentum_x_aux;
            sparse_array[6 * j + 1] = - max_momentum_y_aux;
            sparse_array[6 * j + 2] = - max_kinetic_energy_aux;
            sparse_array[6 * j + 3] = - max_lateral_curvature_aux;
            sparse_array[6 * j + 4] = - max_shear_stress_x_aux;
            sparse_array[6 * j + 5] = - max_shear_stress_y_aux;

            /* Asing error */
            if (j == 0) {
                max_momentum_x = max_momentum_x_aux;
                max_momentum_y = max_momentum_y_aux;
                max_kinetic_energy = max_kinetic_energy_aux;
                max_lateral_curvature = max_lateral_curvature_aux;
                max_shear_stress_x = max_shear_stress_x_aux;
                max_shear_stress_y = max_shear_stress_y_aux;
            } else {
                if (max_momentum_x_aux > max_momentum_x) max_momentum_x = max_momentum_x_aux;
                if (max_momentum_y_aux > max_momentum_y) max_momentum_y = max_momentum_y_aux;
                if (max_kinetic_energy_aux > max_kinetic_energy) max_kinetic_energy = max_kinetic_energy_aux;
                if (max_lateral_curvature_aux > max_lateral_curvature) max_lateral_curvature = max_lateral_curvature_aux;
                if (max_shear_stress_x_aux > max_shear_stress_x) max_shear_stress_x = max_shear_stress_x_aux;
                if (max_shear_stress_y_aux > max_shear_stress_y) max_shear_stress_y = max_shear_stress_y_aux;
            }

            /* Save current face params */
            params_aux = params[j];

            //---------------------------//
            /*          delta            */
            //---------------------------//
            /* Add delta eps params */
            params[j] = params_delta_eps[j];

            /* Calculate divergent and gradient */
            calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

            /* Obj. func. with delta eps */
            calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * j); // momentum x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * j); // momentum y
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * j); // kinetic energy
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * j); // lateral curvature
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * j); // shear stress x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * j); // shear stress y

            //---------------------------//
            /*            A              */
            //---------------------------//
            /* Add delta eps params */
            params[j] = params_A_eps[j];

            /* Calculate divergent and gradient */
            calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

            /* Obj. func. with delta eps */
            calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * j + 1); // momentum x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * j + 1); // momentum y
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * j + 1); // kinetic energy
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * j + 1); // lateral curvature
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * j + 1); // shear stress x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * j + 1); // shear stress y

            //---------------------------//
            /*            B              */
            //---------------------------//
            /* Add delta eps params */
            params[j] = params_B_eps[j];

            /* Calculate divergent and gradient */
            calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

            /* Obj. func. with delta eps */
            calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * j + 2); // momentum x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * j + 2); // momentum y
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * j + 2); // kinetic energy
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * j + 2); // lateral curvature
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * j + 2); // shear stress x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * j + 2); // shear stress y

            //---------------------------//
            /*           Psi             */
            //---------------------------//
            /* Add delta eps params */
            params[j] = params_Psi_eps[j];

            /* Calculate divergent and gradient */
            calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

            /* Obj. func. with delta eps */
            calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * j + 3); // momentum x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * j + 3); // momentum y
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * j + 3); // kinetic energy
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * j + 3); // lateral curvature
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * j + 3); // shear stress x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * j + 3); // shear stress y

            //---------------------------//
            /*          Ctau1            */
            //---------------------------//
            /* Add delta eps params */
            params[j] = params_Ctau1_eps[j];

            /* Calculate divergent and gradient */
            calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

            /* Obj. func. with delta eps */
            calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * j + 4); // momentum x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * j + 4); // momentum y
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * j + 4); // kinetic energy
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * j + 4); // lateral curvature
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * j + 4); // shear stress x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * j + 4); // shear stress y

            //---------------------------//
            /*          Ctau2            */
            //---------------------------//
            /* Add delta eps params */
            params[j] = params_Ctau2_eps[j];

            /* Calculate divergent and gradient */
            calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

            /* Obj. func. with delta eps */
            calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * j + 5); // momentum x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * j + 5); // momentum y
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * j + 5); // kinetic energy
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * j + 5); // lateral curvature
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * j + 5); // shear stress x
            addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * j + 5); // shear stress y

            params[j] = params_aux;

            /* Calculate objective function */
            for (k = 0; k < faces_connection[j].n; k++) {

                /* Save current face params */
                params_aux = params[faces_connection[j].faces[k]];

                //---------------------------//
                /*          delta            */
                //---------------------------//

                /* Add delta eps params */
                params[faces_connection[j].faces[k]] = params_delta_eps[faces_connection[j].faces[k]];

                /* Calculate divergent and gradient */
                calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
                calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

                /* Obj. func. with delta eps */
                calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * faces_connection[j].faces[k]); // momentum x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * faces_connection[j].faces[k]); // momentum y
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * faces_connection[j].faces[k]); // kinetic energy
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * faces_connection[j].faces[k]); // lateral curvature
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * faces_connection[j].faces[k]); // shear stress x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * faces_connection[j].faces[k]); // shear stress y

                //---------------------------//
                /*            A              */
                //---------------------------//

                /* Add delta eps params */
                params[faces_connection[j].faces[k]] = params_A_eps[faces_connection[j].faces[k]];

                /* Calculate divergent and gradient */
                calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
                calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

                /* Obj. func. with delta eps */
                calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * faces_connection[j].faces[k] + 1); // momentum x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * faces_connection[j].faces[k] + 1); // momentum y
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * faces_connection[j].faces[k] + 1); // kinetic energy
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * faces_connection[j].faces[k] + 1); // lateral curvature
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * faces_connection[j].faces[k] + 1); // shear stress x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * faces_connection[j].faces[k] + 1); // shear stress y

                //---------------------------//
                /*            B              */
                //---------------------------//

                /* Add delta eps params */
                params[faces_connection[j].faces[k]] = params_B_eps[faces_connection[j].faces[k]];

                /* Calculate divergent and gradient */
                calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
                calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

                /* Obj. func. with delta eps */
                calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * faces_connection[j].faces[k] + 2); // momentum x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * faces_connection[j].faces[k] + 2); // momentum y
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * faces_connection[j].faces[k] + 2); // kinetic energy
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * faces_connection[j].faces[k] + 2); // lateral curvature
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * faces_connection[j].faces[k] + 2); // shear stress x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * faces_connection[j].faces[k] + 2); // shear stress y

                //---------------------------//
                /*           Psi             */
                //---------------------------//

                /* Add delta eps params */
                params[faces_connection[j].faces[k]] = params_Psi_eps[faces_connection[j].faces[k]];

                /* Calculate divergent and gradient */
                calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
                calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

                /* Obj. func. with delta eps */
                calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * faces_connection[j].faces[k] + 3); // momentum x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * faces_connection[j].faces[k] + 3); // momentum y
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * faces_connection[j].faces[k] + 3); // kinetic energy
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * faces_connection[j].faces[k] + 3); // lateral curvature
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * faces_connection[j].faces[k] + 3); // shear stress x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * faces_connection[j].faces[k] + 3); // shear stress y

                //---------------------------//
                /*          Ctau1            */
                //---------------------------//

                /* Add delta eps params */
                params[faces_connection[j].faces[k]] = params_Ctau1_eps[faces_connection[j].faces[k]];

                /* Calculate divergent and gradient */
                calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
                calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

                /* Obj. func. with delta eps */
                calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * faces_connection[j].faces[k] + 4); // momentum x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * faces_connection[j].faces[k] + 4); // momentum y
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * faces_connection[j].faces[k] + 4); // kinetic energy
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * faces_connection[j].faces[k] + 4); // lateral curvature
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * faces_connection[j].faces[k] + 4); // shear stress x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * faces_connection[j].faces[k] + 4); // shear stress y

                //---------------------------//
                /*          Ctau2            */
                //---------------------------//

                /* Add delta eps params */
                params[faces_connection[j].faces[k]] = params_Ctau2_eps[faces_connection[j].faces[k]];

                /* Calculate divergent and gradient */
                calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
                calculateGradients(j, faces, vertices_connection, params, e1, e2, e3, p1, p2, p3, velNorm, velx, vely, velz, transpiration);

                /* Obj. func. with delta eps */
                calculateObjectiveFunction(params[j], &max_momentum_x_aux, &max_momentum_y_aux, &max_kinetic_energy_aux, &max_lateral_curvature_aux, &max_shear_stress_x_aux, &max_shear_stress_y_aux, velNorm[j]);

                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_x_aux + sparse_array[6 * j]) / eps, 6 * j, 6 * faces_connection[j].faces[k] + 5); // momentum x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_momentum_y_aux + sparse_array[6 * j + 1]) / eps, 6 * j + 1, 6 * faces_connection[j].faces[k] + 5); // momentum y
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_kinetic_energy_aux + sparse_array[6 * j + 2]) / eps, 6 * j + 2, 6 * faces_connection[j].faces[k] + 5); // kinetic energy
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_lateral_curvature_aux + sparse_array[6 * j + 3]) / eps, 6 * j + 3, 6 * faces_connection[j].faces[k] + 5); // lateral curvature
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_x_aux + sparse_array[6 * j + 4]) / eps, 6 * j + 4, 6 * faces_connection[j].faces[k] + 5); // shear stress x
                addSparseValue(sparse_a, sparse_ia, sparse_ja, &index_sparse, (max_shear_stress_y_aux + sparse_array[6 * j + 5]) / eps, 6 * j + 5, 6 * faces_connection[j].faces[k] + 5); // shear stress y

                /* Return to previous params */
                params[faces_connection[j].faces[k]] = params_aux;

            }
        }

        /* Solve system */
        if (i == 0) for (j = 0; j < 6 * nf; j++) increase[j] = 0.0;

        max_sparse_array = 0.0;
        for (l = 0; l < 6 * nf; l++) max_sparse_array = max_sparse_array + pow(sparse_array[l], 2);
        max_sparse_array = sqrt(max_sparse_array);
        for (l = 0; l < 6 * nf; l++) sparse_array[l] = sparse_array[l] / max_sparse_array;
        for (l = 0; l < size_sparse_a; l++) sparse_a[l] = sparse_a[l] / max_sparse_array;
        
        solveSparseSystem(6 * nf, size_sparse_a, sparse_a, sparse_ia, sparse_ja, sparse_array, increase);

        /* Increase parameters */
        for (j = 0; j < nf; j++) {
            norm_delta_list[j] = norm_delta_list[j] + 0.1 * increase[6 * j];
            norm_A_list[j] = norm_A_list[j] + 0.1 * increase[6 * j + 1];
            norm_B_list[j] = norm_B_list[j] + 0.1 * increase[6 * j + 2];
            norm_Psi_list[j] = norm_Psi_list[j] + 0.1 * increase[6 * j + 3];
            norm_Ctau1_list[j] = norm_Ctau1_list[j] + 0.1 * increase[6 * j + 4];
            norm_Ctau2_list[j] = norm_Ctau2_list[j] + 0.1 * increase[6 * j + 5];
        }

        /* Print error */
        if (i < 9) {
            printf("           %d        %.4e       %.4e      %.4e       %.4e      %.4e      %.4e\n", i + 1, max_momentum_x, max_momentum_y, max_kinetic_energy, max_lateral_curvature, max_shear_stress_x, max_shear_stress_y);
        } else if ((i > 8) && (i < 99)) {
            printf("           %d       %.4e       %.4e      %.4e       %.4e      %.4e      %.4e\n", i + 1, max_momentum_x, max_momentum_y, max_kinetic_energy, max_lateral_curvature, max_shear_stress_x, max_shear_stress_y);
        } else if ((i > 99) && (i < 999)) {
            printf("           %d      %.4e       %.4e      %.4e       %.4e      %.4e      %.4e\n", i + 1, max_momentum_x, max_momentum_y, max_kinetic_energy, max_lateral_curvature, max_shear_stress_x, max_shear_stress_y);
        } else {
            printf("           %d     %.4e       %.4e      %.4e       %.4e      %.4e      %.4e\n", i + 1, max_momentum_x, max_momentum_y, max_kinetic_energy, max_lateral_curvature, max_shear_stress_x, max_shear_stress_y);
        }

        if (i == 10) break;
        
        /* Calculate inviscid parameters */
        // if (((i + 1) % 5) == 0) {
        if (i > 3000) {

            /* Params */
            for (j = 0; j < nf; j++) {
                freestream.velocity = velNorm[j];
                freestream.mach = mach[j];
                calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * norm_A_list[j], norm_B * norm_B_list[j], norm_Psi * norm_Psi_list[j], norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params[j]);
            }

            /* Divergents */
            for (j = 0; j < nf; j++) {
                calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
            }

            /* Transpiration */
            for (j = 0; j < nf; j++) {
                transpiration[j] = absValue(params[j].div_M) / density;
                tau_x[j] = params[j].tau_w_x;
                tau_y[j] = params[j].tau_w_y;
            }

            /* Solve linear system with zero transpiration */
            calculateDoubletDistribution(nf, matrix, array, transpiration, doublet);

            /* Calculate potential surface parameters */
            calculateSurfaceParameters(nf, matrixVelx, matrixVely, matrixVelz, arrayVel, doublet, freestreamNorm, velx, vely, velz, velNorm, cp, mach, sound_speed);
        }
    }

    for (j = 0; j < nf; j++) {
        freestream.velocity = velNorm[j];
        freestream.mach = mach[j];
        calculateEquationsParams(norm_delta * norm_delta_list[j], norm_A * norm_A_list[j], norm_B * norm_B_list[j], norm_Psi * norm_Psi_list[j], norm_Ctau1 * norm_Ctau1_list[j], norm_Ctau2 * norm_Ctau2_list[j], &freestream, &profiles, &integralThickness, &integralDefect, &params[j]);
    }

    /* Divergents */
    for (j = 0; j < nf; j++) {
        calculateDivergents(j, faces, vertices_connection, params, facesArea[j], p1, p2, p3);
    }

    /* Transpiration */
    for (j = 0; j < nf; j++) {
        transpiration[j] = absValue(params[j].div_M) / density;
        tau_x[j] = params[j].tau_w_x;
        tau_y[j] = params[j].tau_w_y;
    }

    /* Assing values */
    for (i = 0; i < nf; i++) {
        delta[i] = norm_delta * norm_delta_list[i];
        A[i] = norm_A * norm_A_list[i];
        B[i] = norm_B * norm_B_list[i];
        Psi[i] = norm_Psi * norm_Psi_list[i];
        Ctau1[i] = norm_Ctau1 * norm_Ctau1_list[i];
        Ctau2[i] = norm_Ctau2 * norm_Ctau2_list[i];
    }

    /* Free arrays */
    free(params);
    free(params_delta_eps);
    free(params_A_eps);
    free(params_B_eps);
    free(params_Psi_eps);
    free(params_Ctau1_eps);
    free(params_Ctau2_eps);

    free(norm_delta_list);
    free(norm_A_list);
    free(norm_B_list);
    free(norm_Psi_list);
    free(norm_Ctau1_list);
    free(norm_Ctau2_list);

    free(profiles.eta);
    free(profiles.U);
    free(profiles.W);
    free(profiles.S);
    free(profiles.T);
    free(profiles.R);
    free(profiles.dU_deta);
    free(profiles.dW_deta);

    free(faces_connection);
}

/*
#####################################################
    SOLVER
#####################################################
*/
void solve(int type,
           int nv,
           int nf,
           double *vertices,
           int *faces,
           double *facesAreas,
           double *facesMaxDistance,
           double *facesCenter,
           double *controlPoints,
           double *p1,
           double *p2,
           double *p3,
           double *e1,
           double *e2,
           double *e3,
           double *freestream,
           double *sigma,
           int nSpanLeftWing,
           int nWakeLeftWing,
           int *leftWingGrid,
           double *leftWingVertices,
           int *leftWingFaces,
           int nSpanRightWing,
           int nWakeRightWing,
           int *rightWingGrid,
           double *rightWingVertices,
           int *rightWingFaces,
           int nSpanTail,
           int nWakeTail,
           int *tailGrid,
           double *tailVertices,
           int *tailFaces,
           double *doublet,
           double *velx, double *vely, double *velz, double *velNorm,
           double *cp,
           double *mach,
           double *delta,
           double *A, double *B,
           double *Psi,
           double *Ctau1, double *Ctau2,
           double *tau_x, double *tau_y, double *tau_z,
           double density, double viscosity, double sound_speed,
           double *transpiration,
           double *sigma_v,
           double *doublet_v,
           double *cp_v,
           double *velx_v, double *vely_v, double *velz_v, double *velNorm_v,
           double *transpiration_v,
           double *delta_v,
           double *A_v, double *B_v,
           double *Psi_v,
           double *Ctau1_v, double *Ctau2_v,
           double *tau_x_v, double *tau_y_v, double *tau_z_v)
{

    printf("Aerodynamic solver\n");

    printf("  - Potential flow\n");

    /* Potential flow parameters */
    double *matrix, *array;
    double *matrixVelx, *matrixVely, *matrixVelz, *arrayVel;
    double freestreamNorm;

    /* Initialize */
    matrix = (double *)malloc(nf * nf * sizeof(double));
    array = (double *)malloc(nf * sizeof(double));
    matrixVelx = (double *)malloc(nf * nf * sizeof(double));
    matrixVely = (double *)malloc(nf * nf * sizeof(double));
    matrixVelz = (double *)malloc(nf * nf * sizeof(double));
    arrayVel = (double *)malloc(nf * 3 * sizeof(double));
    freestreamNorm = sqrt(freestream[0] * freestream[0] + freestream[1] * freestream[1] + freestream[2] * freestream[2]);

    /* Potential flow system */
    printf("    > Creating linear system\n");
    createLinearSystem(nf, facesAreas, facesMaxDistance, facesCenter, controlPoints, p1, p2, p3, e1, e2, e3, freestream, sigma, nSpanLeftWing, nWakeLeftWing, leftWingGrid, leftWingVertices, leftWingFaces, nSpanRightWing, nWakeRightWing, rightWingGrid, rightWingVertices, rightWingFaces, nSpanTail, nWakeTail, tailGrid, tailVertices, tailFaces, matrix, array, matrixVelx, matrixVely, matrixVelz, arrayVel);

    /* Solve linear system with zero transpiration */
    printf("    > Solving linear system\n");
    calculateDoubletDistribution(nf, matrix, array, transpiration, doublet);

    /* Calculate potential surface parameters */
    calculateSurfaceParameters(nf, matrixVelx, matrixVely, matrixVelz, arrayVel, doublet, freestreamNorm, velx, vely, velz, velNorm, cp, mach, sound_speed);

    /* Boundary layer correction */

    /* Boundary layer parameters */
    double *tau_wall_x, *tau_wall_y;
    struct VerticeConnection *vertices_connection;

    /* Initialize */
    tau_wall_x = (double *)calloc(nf, sizeof(double));
    tau_wall_y = (double *)calloc(nf, sizeof(double));
    vertices_connection = (struct VerticeConnection *)malloc(nv * sizeof(struct VerticeConnection));

    calculateVerticesConnection(nv, nf, vertices, faces, vertices_connection);

    if (type == 1)
    {
        printf("  - Boundary layer correction\n");
        solveBoundaryLayer(nf, nv, vertices_connection, vertices, faces, facesCenter, facesAreas, e1, e2, e3, p1, p2, p3, transpiration, delta, A, B, Psi, Ctau1, Ctau2, tau_wall_x, tau_wall_y, velNorm, velx, vely, velz, mach, density, viscosity, cp, sound_speed, matrix, array, matrixVelx, matrixVely, matrixVelz, arrayVel, doublet, freestreamNorm);
        printf("\n");
    }

    /* Find faces and vertices values */

    /* Parameters */
    int i, j;
    struct Point e3_point, vel_point, s1, s2;
    double aux;

    // Faces
    for (i = 0; i < nf; i++)
    {

        e3_point.x = e3[3 * i];
        e3_point.y = e3[3 * i + 1];
        e3_point.z = e3[3 * i + 2];

        vel_point.x = velx[i] - transpiration[i] * e3_point.x;
        vel_point.y = vely[i] - transpiration[i] * e3_point.y;
        vel_point.z = velz[i] - transpiration[i] * e3_point.z;

        aux = norm(vel_point);

        s1.x = vel_point.x / aux;
        s1.y = vel_point.y / aux;
        s1.z = vel_point.z / aux;

        s2 = cross(s1, e3_point);

        tau_x[i] = s1.x * tau_wall_x[i] + s2.x * tau_wall_y[i];
        tau_y[i] = s1.y * tau_wall_x[i] + s2.y * tau_wall_y[i];
        tau_z[i] = s1.z * tau_wall_x[i] + s2.z * tau_wall_y[i];
    }

    // Vertices
    for (i = 0; i < nv; i++)
    {

        sigma_v[i] = 0.0;
        doublet_v[i] = 0.0;
        cp_v[i] = 0.0;
        velx_v[i] = 0.0;
        vely_v[i] = 0.0;
        velz_v[i] = 0.0;
        velNorm_v[i] = 0.0;
        transpiration_v[i] = 0.0;
        delta_v[i] = 0.0;
        A_v[i] = 0.0;
        B_v[i] = 0.0;
        Psi_v[i] = 0.0;
        Ctau1_v[i] = 0.0;
        Ctau2_v[i] = 0.0;
        tau_x_v[i] = 0.0;
        tau_y_v[i] = 0.0;
        tau_z_v[i] = 0.0;

        for (j = 0; j < vertices_connection[i].n; j++)
        {
            sigma_v[i] = sigma_v[i] + sigma[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            doublet_v[i] = doublet_v[i] + doublet[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            cp_v[i] = cp_v[i] + cp[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            velx_v[i] = velx_v[i] + velx[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            vely_v[i] = vely_v[i] + vely[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            velz_v[i] = velz_v[i] + velz[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            velNorm_v[i] = velNorm_v[i] + velNorm[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            transpiration_v[i] = transpiration_v[i] + transpiration[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            delta_v[i] = delta_v[i] + delta[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            A_v[i] = A_v[i] + A[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            B_v[i] = B_v[i] + B[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            Psi_v[i] = Psi_v[i] + Psi[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            Ctau1_v[i] = Ctau1_v[i] + Ctau1[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            Ctau2_v[i] = Ctau2_v[i] + Ctau2[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            tau_x_v[i] = tau_x_v[i] + tau_x[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            tau_y_v[i] = tau_y_v[i] + tau_y[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
            tau_z_v[i] = tau_z_v[i] + tau_z[vertices_connection[i].faces[j]] * vertices_connection[i].coeffs[j];
        }
    }

    free(matrix);
    free(array);
    free(matrixVelx);
    free(matrixVely);
    free(matrixVelz);
    free(arrayVel);
}