#ifndef SHARING_H_
#define SHARING_H_

struct PointStruct{
    int row;
    int column;
};

typedef struct PointStruct Point;

struct AgentStruct{
    int systemAgentNumber;
    char *agentSymbols;
    int currentAgentNumber;
    Point mapDimensions;
    char** map;
};

typedef struct AgentStruct Agent;

//ERROR DEFINES
#define GOAL 0
#define INVALID_PARAMS_NO 1
#define INVALID_PARAMS 2
#define INVALID_DEPENDENCIES 3
#define BAD_HANDLER_COMM 4
//

//Function Prototypes
int check_start_inputs(Agent* );
int run_get_map(Agent* );
int check_map(Agent* );
int display_agent_data(Agent* );

int get_coordinates(Agent* , Point* );
int check_direction_with_map(Agent* , char, Point);
int display_map(Agent* );

ssize_t get_line(char **lineptr, size_t *n, FILE *stream);
void error(int);

#endif
