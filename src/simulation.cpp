#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>

#include <casadi/casadi.hpp>
#include <boat_model.h>

using namespace casadi;
using namespace bifoiler;

void csv_write_line(std::ofstream &out, DM x)
{
    for (int i = 0; i < x.size1(); i++) {
        if (i != 0) {
            out << ",";
        }
        out << x(i);
    }
    out << "\n";
}

DM cvodes_solve(Function &cvodes_integrator, const DM &x0, const DM &u)
{
    DMDict out;
    try
    {
        DMDict args = {{"x0", x0}, {"p", u}};
        out = cvodes_integrator(args);
    }
    catch(std::exception &e)
    {
        std::cout << "At state x0 : " << x0 << "\n";
        std::cout << "At control u:" << u << "\n";
        std::cout << "CVODES exception " << e.what() << "\n";
    }

    return out["xf"];
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cout << "usage: " << argv[0] << " config.yaml [output/dir]" << std::endl;
        return -1;
    }

    std::ofstream sim_x("sim_x.csv");
    std::ofstream sim_u("sim_u.csv");
    // std::ofstream sim_z("sim_z.csv");

    sim_x << "vx,vy,vz,wx,wy,wz,rx,ry,rz,q0,q1,q2,q3\n";
    sim_u << "flaps,aileron,rudder,thrust\n";
    // sim_z << "vx,vy,vz,wx,wy,wz,rx,ry,rz,ax,ay,az\n";

    std::string config_file(argv[1]);
    BoatProperties prop = BoatProperties::Load(config_file);

    BoatDynamics boat_model(prop);

    SX dynamics = boat_model.getSymbolicDynamics();
    SX state = boat_model.getSymbolicState();
    SX control = boat_model.getSymbolicControl();   // Control: Flaps, Ailerons, Rudder

    const double h = prop.estimator.t_samp;
    SXDict ode = {{"x", state}, {"p", control}, {"ode", dynamics}};
    Dict opts = {{"tf", h}};
    Function CVODES_INT = integrator("CVODES_INT", "cvodes", ode, opts);

    const double SIM_TIME = 20; // [s]
    DM x = DM::vertcat({
        5, 0, 0,    // v0 [m/s] in BRF
        0, 0, 0,    // w0 [rad/s]
        0, 0, -0.2, // r0 [m] in IRF (NED)
        1, 0, 0, 0  // q0
    });

    DM u = DM::vertcat({0, 0, 0, 0.5}); // Flaps, Aileron, Rudder, Thrust

    for (double t = 0; t < SIM_TIME; t += h) {
        std::cout << x << std::endl;
        csv_write_line(sim_x, x);
        csv_write_line(sim_u, u);
        x = cvodes_solve(CVODES_INT, x, u);
    }
}