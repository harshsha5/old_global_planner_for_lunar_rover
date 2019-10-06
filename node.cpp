//
// Created by Harsh Sharma on 06/10/19.
//

#include "node.h"
#include <cmath>
#include "map_width_header.h"

double Node::calculate_fcost()
{
    return gcost + hcost;
}

//=====================================================================================================================

double Node::calculate_hcost(const coordinate &goal_coordinate)
{
    return (sqrt((this->c.x - goal_coordinate.x)*(this->c.x - goal_coordinate.x)
                             + (this->c.y - goal_coordinate.y)*(this->c.y - goal_coordinate.y)));
}

//=====================================================================================================================

void Node::set_fcost(const double &new_fcost)
{
    fcost = new_fcost;
}

//=====================================================================================================================

void Node::set_gcost(const double &new_gcost)
{
   gcost = new_gcost;
   Node::set_fcost(calculate_fcost());
}

//=====================================================================================================================

void Node::set_hcost(const double &new_hcost)
{
    hcost = new_hcost;
    Node::set_fcost(calculate_fcost());
}

//=====================================================================================================================

void Node::print_node()
{
    c.print_coordinate();
    cout<<"gcost: "<<gcost<<"\t"<<"hcost: "<<hcost<<"\t"<<"fcost: "<<fcost<<endl;
}

//=====================================================================================================================

size_t node_hasher::operator()(const Node &obj) const
{
    return std::hash<int>()(obj.c.x * GLOBAL_MAP_WIDTH + obj.c.y);
}

//=====================================================================================================================
