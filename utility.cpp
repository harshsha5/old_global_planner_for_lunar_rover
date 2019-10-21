//
// Created by Harsh Sharma on 06/10/19.
//

#include "utility.h"
#include "planning_map.h"
#include "node.h"
#include <iostream>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include "mga_node.h"
#include "map_width_header.h"

int GLOBAL_MAP_WIDTH;

//=====================================================================================================================

bool is_destination(const coordinate &c, const coordinate &goal)
{
    return c == goal;

}

//=====================================================================================================================

void expand_state(const Node &node_to_expand,
                  priority_queue<Node, vector<Node>, Comp> &open,
                  vector<vector<Node>> &node_map,
                  const vector<int> &dX,
                  const vector<int> &dY,
                  const planning_map &elevation_map,
                  unordered_set<Node,node_hasher> &closed)
{
    const auto current_x = node_to_expand.c.x;
    const auto current_y = node_to_expand.c.y;
    const int cost_per_step = 1;
    for(size_t dir = 0; dir < dX.size(); dir++)
    {
        int newx = current_x + dX[dir];
        int newy = current_y + dY[dir];
        coordinate new_coordinate {newx,newy};

        if (elevation_map.is_valid(new_coordinate))
        {
            if(!closed.count(node_map[newx][newy]) && (node_map[newx][newy].gcost > node_map[current_x][current_y].gcost + cost_per_step))
            {
                node_map[newx][newy].set_gcost(node_map[current_x][current_y].gcost + cost_per_step);
                node_map[newx][newy].set_parent(node_to_expand.c);
                open.push(node_map[newx][newy]);
            }
        }
    }
}

//=====================================================================================================================

vector<coordinate> backtrack(  const Node &start_node,
                                const Node &goal_node,
                                vector<vector<Node>> &node_map)
{
    vector<coordinate> path;
    Node curr_node = goal_node;
    while(curr_node!=start_node)
    {
        path.emplace_back(curr_node.c);
        curr_node = node_map[curr_node.parent.x][curr_node.parent.y];
    }
    std::reverse(path.begin(),path.end());
    return std::move(path);
}

//=====================================================================================================================

vector<coordinate> astar(const coordinate &start,const coordinate &goal,const planning_map &elevation_map){

    vector<coordinate> path{};
    if (!elevation_map.is_valid(goal)) {
        std::cout << "Destination is an obstacle" << std::endl;
        return path;
        //Destination is invalid
    }

    if (is_destination(start, goal)) {
        std::cout << "Already at the destination" << std::endl;
        return path;
    }
    GLOBAL_MAP_WIDTH =  elevation_map.map[0].size();
    const vector<int> dX = {-1, -1, -1,  0,  0,  1, 1, 1};
    const vector<int> dY {-1,  0,  1, -1,  1, -1, 0, 1};

    const Node random_init_node{coordinate{-1,-1},coordinate{-1,-1},INT_MAX,INT_MAX};
    vector<vector<Node>> node_map(elevation_map.map.size(),vector<Node> (elevation_map.map[0].size(),random_init_node));
    for(int i=0;i<elevation_map.map.size();i++)
    {
        for(int j=0;j<elevation_map.map[0].size();j++)
        {
            node_map[i][j].c = coordinate{i,j};
            node_map[i][j].set_hcost(node_map[i][j].calculate_hcost(goal));
        }
    }

    priority_queue<Node, vector<Node>, Comp> open;
    unordered_set<Node,node_hasher> closed;

    Node start_node(start,coordinate{-1,-1},0,INT_MAX);
    start_node.set_hcost(start_node.calculate_hcost(goal));
    Node goal_node(goal,coordinate{-1,-1},INT_MAX,0);
    node_map[start_node.c.x][start_node.c.y] = start_node;
    node_map[goal_node.c.x][goal_node.c.y] = goal_node;

    open.push(start_node);
    while (!open.empty() && !closed.count(goal_node))
    {
        const auto node_to_expand = open.top();
        open.pop();
        if(closed.count(node_to_expand)==0)       //Added this new condition to avoid multiple expansion of the same state
        {
            closed.insert(node_to_expand);
            expand_state(node_to_expand,open,node_map,dX,dY,elevation_map,closed);
        }
    }
    path = backtrack(node_map[start_node.c.x][start_node.c.y],node_map[goal_node.c.x][goal_node.c.y],node_map);
    return std::move(path);
}

//=====================================================================================================================

struct multi_goal_A_star_configuration
{
    double pessimistic_factor;
    double time_difference_weight;
    explicit multi_goal_A_star_configuration(double pessismistic_time_factor = 0.5,double weight_time_difference = 2):
            pessimistic_factor(pessismistic_time_factor),time_difference_weight(weight_time_difference){}

};

//=====================================================================================================================

double get_MGA_heuristic(const coordinate &start_coordinate,
                         const vector<coordinate> &goals)
{
    /// This heuristic is Euclidian as of now.
    double min_heuristic = INT_MAX;
    for(const auto &goal:goals)
    {
        min_heuristic = std::min(min_heuristic,start_coordinate.get_euclidian_distance(goal));
    }
    return min_heuristic;
}

//=====================================================================================================================

void implicit_expand_state(const MGA_Node  &node_to_expand,
                          priority_queue<MGA_Node, vector<MGA_Node>, MGA_Comp> &open,
                          const vector<int> &dX,
                          const vector<int> &dY,
                          const planning_map &elevation_map,
                          unordered_set<MGA_Node,MGA_node_hasher> &closed,
                          unordered_set<MGA_Node,MGA_node_hasher> &node_map,
                           const vector<coordinate> &goals,
                          const rover_parameters &rover_config)
{
    const auto current_x = node_to_expand.n.c.x;
    const auto current_y = node_to_expand.n.c.y;
    const double cost_per_step = 1;                    //See if you want to change the diagonal traversal value
    const double pixel_distance = 5;                //This is the per pixel distance/discretization
    const double time_of_traversal_per_pixel= pixel_distance/rover_config.velocity;

    for(size_t dir = 0; dir < dX.size(); dir++)
    {
        int newx = current_x + dX[dir];
        int newy = current_y + dY[dir];
        coordinate new_coordinate {newx,newy};
        const MGA_Node temp_mga_node{new_coordinate};

        if (elevation_map.is_valid(new_coordinate) && !closed.count(temp_mga_node))
        {

            double present_g_cost = INT_MAX;
            double transition_cost = cost_per_step;
            double traversal_time = time_of_traversal_per_pixel;

            auto node_in_consideration = node_map.find (temp_mga_node);
            if(node_in_consideration!=node_map.end())
            {
                present_g_cost = node_in_consideration->n.gcost;
            }

            if(abs(dX[dir]) && abs(dY[dir]))
            {
                traversal_time = 1.4*time_of_traversal_per_pixel;
                transition_cost =  1.4*cost_per_step;
            }

            if(present_g_cost > node_to_expand.n.gcost + transition_cost)
            {
                if(node_in_consideration!=node_map.end())
                {
                    node_map.erase(node_in_consideration);      //Node map is an unordered set. An existing element has to be deleted, it can't be changed.
                }

                double new_gcost = node_to_expand.n.gcost + transition_cost;
                double new_hcost = get_MGA_heuristic(new_coordinate,goals);
                MGA_Node new_MGA_node{new_coordinate,node_to_expand.n.c,new_gcost,new_hcost,node_to_expand.time_to_reach+traversal_time};
                node_map.insert(new_MGA_node);
                //Validate if gcost,hcost and fcost are getting updated
                open.push(new_MGA_node);

            }
        }
    }
}

//=====================================================================================================================

MGA_Node get_best_goal(unordered_map<MGA_Node,double,MGA_node_hasher> &goal_traversal_times,
                       const multi_goal_A_star_configuration &MGA_config,
                       const vector<double> &time_remaining_to_lose_vantage_point_status,
                       bool &vantage_point_reached_within_time,
                       const vector<coordinate> &goals)
{
    double best_time_stat = INT_MIN;
    MGA_Node best_goal{coordinate{-1,-1}};
    for(size_t i=0;i<time_remaining_to_lose_vantage_point_status.size();i++)
    {
        MGA_Node temp_mga_node{goals[i]};
        auto node_in_consideration = goal_traversal_times.find (temp_mga_node);
        cout<<"Time taken to reach: "<<node_in_consideration->second<<endl;
        cout<<"time_remaining_to_lose_vantage_point_status: "<< time_remaining_to_lose_vantage_point_status[i]<<endl;
        cout<<"Difference inclusive of pessimistic factor "<<MGA_config.pessimistic_factor*time_remaining_to_lose_vantage_point_status[i] - node_in_consideration->second<<endl;
        node_in_consideration->first.print_MGA_node();
        cout<<"============================================================="<<endl;
        if(MGA_config.pessimistic_factor*time_remaining_to_lose_vantage_point_status[i] - node_in_consideration->second > best_time_stat)
        {
            best_time_stat = MGA_config.pessimistic_factor*time_remaining_to_lose_vantage_point_status[i] - node_in_consideration->second;
            best_goal = node_in_consideration->first;
        }
    }

    if(best_time_stat>0)
        vantage_point_reached_within_time=true;

    cout<<"Best time stat is: "<<best_time_stat<<endl;
    best_goal.print_MGA_node();
    return best_goal;
}

//=====================================================================================================================

vector<coordinate> multi_goal_astar(const coordinate &start,
                                  const vector<coordinate> &goals,
                                  const planning_map &elevation_map,
                                  const vector<double> &time_remaining_to_lose_vantage_point_status,
                                  const rover_parameters &rover_config,
                                  const multi_goal_A_star_configuration &MGA_config)
{
    ///Handle case where start point is possible goal point as well. Worst case prune it from the list of possible goals.

    //GLOBAL_MAP_WIDTH =  elevation_map.map[0].size();
    const vector<int> dX = {-1, -1, -1,  0,  0,  1, 1, 1};
    const vector<int> dY {-1,  0,  1, -1,  1, -1, 0, 1};

    priority_queue<MGA_Node, vector<MGA_Node>, MGA_Comp> open;
    unordered_set<MGA_Node,MGA_node_hasher> closed;
    unordered_set<MGA_Node,MGA_node_hasher> goals_set;
    unordered_set<MGA_Node,MGA_node_hasher> node_map;   //This serves as my map since it's an implicit graph
    unordered_map<MGA_Node,double,MGA_node_hasher> goal_traversal_times;

    for(const auto &goal:goals)
    {
        goals_set.insert(MGA_Node{goal});
    }
    MGA_Node start_node(start,coordinate{-1,-1},0,INT_MAX,0);
    start_node.n.set_hcost(get_MGA_heuristic(start,goals));
    open.push(start_node);
    node_map.insert(start_node);

    int goals_expanded = 0;
    while (!open.empty() && goals_expanded!=goals.size())
    {
        const auto node_to_expand = open.top();

        if(goals_set.count(node_to_expand))
        {
            goals_expanded++;
            goal_traversal_times[node_to_expand] = node_to_expand.time_to_reach;
        }

        open.pop();
        if(closed.count(node_to_expand)==0)       //Added this new condition to avoid multiple expansion of the same state
        {
            closed.insert(node_to_expand);
            implicit_expand_state(node_to_expand,open,dX,dY,elevation_map,closed,node_map,goals,rover_config);
        }
    }
    cout<<"Open size is: "<<open.size()<<endl;
    bool vantage_point_reached_within_time = false;
    auto best_goal = get_best_goal(goal_traversal_times,MGA_config,time_remaining_to_lose_vantage_point_status,vantage_point_reached_within_time,goals);
    //path = backtrack(node_map[start_node.c.x][start_node.c.y],node_map[goal_node.c.x][goal_node.c.y],node_map);
    //return std::move(path);
    return goals;
}

//=====================================================================================================================

vector<coordinate> get_path_to_vantage_point(const vector<vector<double>> &g_map,
                                             const double &min_elevation,
                                             const double &max_elevation,
                                             const coordinate &start_coordinate,
                                             const vector<coordinate> &goal_coordinates,
                                             const vector<double> &time_remaining_to_lose_vantage_point_status,
                                             const rover_parameters &rover_config)
{
    planning_map my_map{g_map,min_elevation,max_elevation}; //Pit interiors have to be made obstacle here. Tune min elevation according to that
    const multi_goal_A_star_configuration MGA_config{0.5,5};
    return multi_goal_astar(start_coordinate,goal_coordinates,my_map,time_remaining_to_lose_vantage_point_status,rover_config,MGA_config);
}

//=====================================================================================================================

std::vector<double> split(const std::string& s, char delimiter)
{
    std::vector<double> result;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        result.push_back(stod(token));
    }
    return result;
}

//=====================================================================================================================

vector<vector<double>> convert_csv_to_vector(const string &file_name)
{
    std::ifstream file(file_name);
    string line;
    string number;
    string temp;
    vector<vector<double>> map;
    int count=0;

    while (getline(file, line,'\n'))
    {
        auto res = split(line,',');
        map.push_back(res);
    }

    cout<<"Map_Rows: "<<map.size()<<endl;
    cout<<"Map_Col: "<<map[0].size()<<endl;
//    //testing
//    for(size_t i=0;i<map.size();i++)
//    {
//        for(size_t j=0;j<map[0].size();j++)
//        {
//            cout<<map[i][j]<<"\t";
//        }
//        cout<<endl;
//    }

    return std::move(map);
}

//=====================================================================================================================

void convert_vector_to_csv(const vector<coordinate> &vec,const string &file_name)
{
    ofstream myfile(file_name);
    int vsize = vec.size();
    for (int n=0; n<vsize; n++)
    {
        myfile << vec[n].x <<","<< vec[n].y << endl;
    }
    cout<<"CSV file created"<<endl;
}

//=====================================================================================================================