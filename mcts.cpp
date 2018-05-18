#include <iostream>
#include <string>
#include <cassert>
#include <vector>
#include <random>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <chrono>

#define cerr std::cerr
#define endl std::endl
#define cout std::cout
#define cin std::cin

#pragma GCC optimize "O3,omit-frame-pointer,inline"

typedef std::chrono::milliseconds::rep TimePoint; // A value in milliseconds

inline TimePoint now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Types
using slowminiboard_t = int[9]; // Encoded as 0 1 or 2 for every position
using miniboard_t = int; // Encoded as a "Board position" in base 3
using board_t = miniboard_t[9]; // Encoded as 9 mini boards
using move_t = int; // Encoded as integer from 0 -> 81 indicated where to place a stone
using movelist_t = std::vector<move_t>;
struct mcnode_t;
typedef struct mcnode_t {
    mcnode_t *next;
    mcnode_t *child;
    mcnode_t *parent; // TODO use info from selection
    move_t mv;
    int visits, unexplored, player;
    float mean;
} mcnode_t;

// Consts
const int BOARD_POSITIONS = 19683;
const int WHITE_WIN = 1;
const int BLACK_WIN = 2;
const int EGALITY = 0;
const int NOT_OVER = -1;
const int NULL_MOVE = -1;
const int MEMSIZE = 500'000'000;

const int POW_THREE[9] = {1, 3, 3*3, 3*3*3, 3*3*3*3, 3*3*3*3*3, 3*3*3*3*3*3, 3*3*3*3*3*3*3, 3*3*3*3*3*3*3*3};

int max_from_move[81]; // Get max board id for board_t from move_t
int min_from_move[81]; // Get which miniboard move was played on
move_t movegen_to_move[81];

movelist_t empty_from_miniboard[BOARD_POSITIONS]; // get empty spaces from miniboards
unsigned long long emptybits_from_miniboard[BOARD_POSITIONS]; // get empty spaces from miniboards

int state_from_miniboard[BOARD_POSITIONS]; // get win info on miniboard

void fast_to_slow(miniboard_t mini, slowminiboard_t value);
int get_winner(slowminiboard_t mini);

// Allocation

mcnode_t memory[MEMSIZE/sizeof(mcnode_t)];
int ptr = 0;
mcnode_t* allocate() {
	ptr = (ptr + 1) % MEMSIZE;
	return &memory[ptr];
}

// Printing
void print_slowboard(slowminiboard_t mini) {
    for(int i = 0 ; i < 9 ; i++) {
        cerr << mini[i];
        if(i%3 == 2) {
            cerr << endl;
        }
    }
}

void print_fastboard(miniboard_t mini) {
    slowminiboard_t val;
    fast_to_slow(mini, val);
    print_slowboard(val);
}

void print_moves(movelist_t moves) {
    for(unsigned int i = 0 ; i < moves.size() ; i++) {
        cerr << moves[i] << " ";
    }
    cerr << endl;
}

void print_wholeboard(board_t mini) {
    slowminiboard_t vals[9];
    for(int i = 0 ; i < 9 ; i++) {
        fast_to_slow(mini[i], vals[i]);
    }
    for(int i = 0 ; i < 9 ; i++) {

        for(int j = 0 ; j < 9 ; j++) {
            int m = (i/3)*3 + j/3;
            int val = vals[m][(i%3)*3 + j%3];
            if(val > 0) {
                cerr << val;
            } else {
                cerr << " ";
            }
            if(j%3 == 2) {
                cout << "|";
            }
        }
        if(i%3 == 2) {
            cout << endl << "---+---+---";
        }
        cerr << endl;
    }
}


void print_wholeboard_filled(board_t mini) {
    slowminiboard_t vals[9];
    for(int i = 0 ; i < 9 ; i++) {
        fast_to_slow(mini[i], vals[i]);
    }
    for(int i = 0 ; i < 9 ; i++) {
        for(int j = 0 ; j < 9 ; j++) {
            int m = (i/3)*3 + j/3;
            int stat = get_winner(vals[m]);
            int val = vals[m][(i%3)*3 + j%3];
            if(stat > 0)
                val = stat;
            if(val > 0) {
                cerr << val;
            } else {
                cerr << " ";
            }
            if(j%3 == 2) {
                cout << "|";
            }
        }
        if(i%3 == 2) {
            cout << endl << "---+---+---";
        }
        cerr << endl;
    }
}

void print_mcnode(mcnode_t* node, int depth) {
    if(depth > 2)
        return;
    for(int i = 0 ; i < depth ; i++) {
        cerr << "  ";
    }
    cerr << node->mv << " " << node->mean << "/" << node->visits << endl;
    if(!node->child)
        return;
    mcnode_t* child = node->child;
    while(child) {
        if(child->visits > 0) {
            print_mcnode(child, depth+1);
        }
        child = child->next;
    }
}

// Init
void init_board(board_t b) {
    for(int i = 0 ; i < 9 ; i++) {
        b[i] = 0;
    }
}

// Type conversions
miniboard_t slow_to_fast(slowminiboard_t mini) {
    miniboard_t value = 0;
    int power = 1;
    for(int i = 0 ; i < 9 ; i++) {
        value += mini[i]*power;
        power *= 3;
    }
    return value;
}

void fast_to_slow(miniboard_t mini, slowminiboard_t value) {
    for(int i = 0 ; i < 9 ; i++) {
        value[i] = mini%3;
        mini /= 3;
    }
}

// Movegen

int get_winner(slowminiboard_t mini) {
    for(int i = 0 ; i < 3 ; i++) {
        if(mini[i] != 0 && mini[i] == mini[i+3] && mini[i] == mini[i+6]) {
            return mini[i];
        }
        if(mini[i*3] != 0 && mini[i*3] == mini[i*3+1] && mini[i*3] == mini[i*3+2]) {
            return mini[i*3];
        }
    }
    if(mini[4] != 0) {
        if(mini[0] == mini[4] && mini[0] == mini[8])
            return mini[4];
        if(mini[2] == mini[4] && mini[2] == mini[6])
            return mini[4];
    }
    for(int i = 0 ; i < 9 ; i++) {
        if(mini[i] == 0) {
            return NOT_OVER;
        }
    }
    return EGALITY;
}


void initPrecalculations() {
    srand(0);
    for(int i = 0 ; i < 81 ; i++) {
        max_from_move[i] = i%3+3*((i%27)/9);
        min_from_move[i] = (i%9)/3+3*(i/27);
        movegen_to_move[i] = max_from_move[i]+min_from_move[i]*9;
    }

    for(int i = 0 ; i < BOARD_POSITIONS ; i++) {
        slowminiboard_t val;
        fast_to_slow(i, val);
        assert(slow_to_fast(val) == i);
        std::vector<int> moves = std::vector<int>();

        // print_slowboard(val);



        state_from_miniboard[i] = get_winner(val);
        int emptybits = 0;
        for(int j = 0 ; j < 9 ; j++) {
            if(val[j] == 0) {
                emptybits += 1<<j;
                moves.push_back(j%3 + (j/3)*9);
            }
        }
        empty_from_miniboard[i] = moves;
        emptybits_from_miniboard[i] = emptybits;
    }
}

// fast movegen
// 1- last move
// 2- maxboard
// 3- state of miniboard
// 4- everywhere | empty spaces, capturing spaces ? Move ordering..
// 5- list of moves to iterate in

movelist_t moves(board_t board, move_t last_move) {
    int maxboard = max_from_move[last_move];
    int state = state_from_miniboard[board[maxboard]];
    movelist_t moves;

    //cerr << "maxb " << maxboard << " st "<<state<<endl;
    //print_fastboard(board[maxboard]);
    //print_wholeboard(board);

    if(state == NOT_OVER) {
        moves = empty_from_miniboard[board[maxboard]];
        //cerr << "empties size "<<moves.size() << endl;

        for(unsigned int i = 0 ; i < moves.size() ; i++) {
            moves[i] += (maxboard%3)*3+(maxboard/3)*27;
        }
    } else {
        moves = movelist_t();
        for(int i = 0 ; i < 9 ; i++) {
            if(state_from_miniboard[board[i]] == NOT_OVER) {
                //cerr << "using board " << i << " ";
                auto empties = empty_from_miniboard[board[i]];
                //cerr << &empties << " " << &empty_from_miniboard << " ";
                for(unsigned int j = 0 ; j < empties.size() ; j++) {
                    empties[j] += (i%3)*3+(i/3)*27;
                    //cerr << empties[j] << " ";
                }
                //cerr << endl;
                moves.insert(moves.end(), empties.begin(), empties.end());
            }
        }
    }
    return moves;
}

void fast_moves(board_t board, move_t last_move,unsigned long long int& first_part, int& second_part) {
    int maxboard = max_from_move[last_move];

    int state = state_from_miniboard[board[maxboard]];

    if(state == NOT_OVER) {
        unsigned long long movesbits = emptybits_from_miniboard[board[maxboard]];
        if(maxboard < 7) {
            first_part |= movesbits << (9*maxboard);
        } else {
            second_part |= movesbits << (9*(maxboard-7));
        }
        //cerr << "not over " << maxboard << " " << movesbits << " " << (movesbits << (9*maxboard)) <<  endl;
    } else {
        for(int i = 0 ; i < 7 ; i++) {
            if(state_from_miniboard[board[i]] == NOT_OVER) {
                //cerr << "using board " << i << " ";
                unsigned long long empties = emptybits_from_miniboard[board[i]];
                first_part |= empties<<(9*i);
            }
        }
        for(int i = 7 ; i < 9 ; i++) {
            if(state_from_miniboard[board[i]] == NOT_OVER) {
                //cerr << "using board " << i << " ";
                int empties = emptybits_from_miniboard[board[i]];
                second_part |= empties<<(9*(i-7));
            }
        }
    }
}

movelist_t get_all_moves() {
    movelist_t moves = movelist_t();
    for(int i = 0 ; i < 81 ; i++) {
        moves.push_back(i);
    }
    return moves;
}
// Utils

int get_status(board_t b) {
    int value = 0;
    int power = 1;
    int at_least_a_negative = 0;
    for(int i = 0 ; i < 9 ; i++) {
        int st = state_from_miniboard[b[i]];
        if(st < 0) {
            at_least_a_negative = 1;
            st = 0;
        }
        value += st*power;
        power *= 3;
    }

    int rs = state_from_miniboard[value];
    return rs < 0 ? -at_least_a_negative : rs;
}

bool is_won(board_t b) {
    return get_status(b) >= 0;
}


void apply_move(board_t board, move_t mov, int player) {
    int maxb = max_from_move[mov];
    int play_id = (player+1)/2+1;
    board[min_from_move[mov]] += POW_THREE[maxb]*play_id;
}

void undo_move(board_t board, move_t mov, int player) {
    int maxb = max_from_move[mov];
    int play_id = (player+1)/2+1;
    board[min_from_move[mov]] -= POW_THREE[maxb]*play_id;
}

std::clock_t start_t;
inline int get_time() {
    return 10000*(std::clock()-start_t)/CLOCKS_PER_SEC;
}

move_t big[81];
move_t not_big[81];
const float C = 1.4f;
// TODO inline?
float get_upper_bound(mcnode_t* node, mcnode_t* parent) {
    return node->mean + C*std::sqrt(2*std::log((float)parent->visits)/node->visits);
}

mcnode_t* pick_uct_node(mcnode_t* root) {
    mcnode_t* best = root->child;
    mcnode_t* iter = best;
    float upper = get_upper_bound(best, root);
    //cerr << "init" << upper << endl;
    while(iter->next) {
        iter = iter->next;
        float upper2 = get_upper_bound(iter, root);
        //cerr << "try" << upper2 << endl;
        if(upper2 > upper) {
            upper = upper2;
            best = iter;
           //cerr << "beast" << upper2 << endl;
        }
    }
    return best;
}

mcnode_t* expand_nodes(mcnode_t* root, board_t b) {
    assert(!root->child);
    movelist_t mvlist = moves(b, root->mv);
    mcnode_t* child = new mcnode_t;
    mcnode_t* random_child;
    int rd = rand()%mvlist.size();
    root->child = child;
    root->unexplored = mvlist.size();
    for(int i = 0 ; i < mvlist.size() ; i++) { // TODO use iterator
        if(i == rd) {
            random_child = child;
        }
        child->child = nullptr;
        child->mv = mvlist[i];
        child->player = -root->player; // -1 <--> 1
        child->visits = 0;
        child->mean = 0;
        child->parent = root;
        if(i < mvlist.size()-1) {
            child->next = new mcnode_t;
            child = child->next;
        } else {
            child->next = nullptr;
        }
    }
    return random_child;
}

int simulate(mcnode_t* node, board_t board) {
    move_t last_move = node->mv;
    board_t cp;
    for(int j = 0 ; j < 9 ; j++)
        cp[j] = board[j];
    int player = -node->player;
    int status;
    do {
        movelist_t mvlist = moves(board, last_move);
        int rd = rand()%mvlist.size();
        apply_move(board, mvlist[rd], player);
        last_move = mvlist[rd];
        player *= -1;
        status = get_status(board);
    }while(status == NOT_OVER);

    for(int j = 0 ; j < 9 ; j++)
        board[j] = cp[j];
    return status;
}

void do_playout(mcnode_t* node, board_t board) {
    // 1. Selection
    mcnode_t* root = node;
    //int depth = 0;
    while(true) {
        //cout << node << " " << node->mv << " " << node->wins << "/" << node->visits << endl;
        if(!node->child) { // is leaf
            //cerr << "Looks like a leaf" << endl;
            break;
        }
        if(node->unexplored > 0) {
            //cerr << "Looks like its unexplored yet" << endl;
            node->unexplored -= 1;
            node = node->child;
            while(node->child) { // Find a leaf
                node = node->next;
            }
            //cerr << "Applying " << node->mv << endl;
            apply_move(board, node->mv, node->player);
            break;
        }
        //cerr << "Picking uct node from " << node << endl;
        node = pick_uct_node(node);
        apply_move(board, node->mv, node->player);
        //cerr << "Applying " << node->mv << endl;
    }

    // 2. Expand
    int status = get_status(board);
    mcnode_t* random_child;
    int result;
    if(status == NOT_OVER) {
        random_child = expand_nodes(node, board);
        // 3. Simulation
        result = simulate(random_child, board); // TODO: Either win or lose ? Should be expected score maybe ? win draw lose..
    } else {
        result = status;// already have result
    }

    if(result == (3+root->player)/2) {
        result = 1;
    } else {
        if(result == 0) {
            result = 0.5f;
        } else {
            result = 0;
        }
    }
    // 4. Backpropagation
    while(node != root) {
        undo_move(board, node->mv, node->player);
        node->visits += 1;
        node->mean += (result - node->mean) / node->visits;
        node = node->parent;
    }
    node->visits += 1;
        node->mean += (result - node->mean) / node->visits;
}

move_t pick_best_move(mcnode_t* root) {
    float most_visits = -1;
    mcnode_t* child = root->child;
    mcnode_t* best = 0;
    while(child) {
        cerr << child->mv << " v: " << child->visits << " w: " << child->mean << " upper: " << get_upper_bound(child, root) << endl;
        if(child->mean > most_visits) {
            most_visits = child->visits;
            best = child;
        }
        child = child->next;
    }

    //getchar();
    if(!best) {
        cerr << root << " " << root->child << " " << most_visits << " " << root->unexplored << endl;
        assert(best);
    }
    return best->mv;
}

move_t get_best_move(board_t b, move_t last_move, int player) {
    mcnode_t root;
    root.child = 0;
    root.mv = last_move;
    root.player = -player;
    root.visits = 0;
    root.mean = 0;
    for(int i = 0 ; i < 1000 ; i++) {
        do_playout(&root, b);
    }
    //print_mcnode(&root, 0);

    //getchar();
    return pick_best_move(&root);
}

void play_CG() {
    cerr << "init done" << endl;
    board_t b;
    init_board(b);
    move_t last_move = NULL_MOVE;
    int player = 1;
    int turn = 0;
    while (1) {
        int opponentRow;
        int opponentCol;
        cin >> opponentRow >> opponentCol; cin.ignore();
        int validActionCount;
        cin >> validActionCount; cin.ignore();

        for (int i = 0; i < validActionCount; i++) {
            int row;
            int col;
            cin >> row >> col; cin.ignore();
        }

        if(opponentRow != -1) {
            last_move = opponentRow*9 + opponentCol;
            apply_move(b, last_move, player);
            player *= -1;
            turn += 1;
        }
        auto tim = std::clock();
        for(int i = 0 ; i < 9 ; i++) {
            cerr << b[i] << " ";
        }
        cerr << endl;
        move_t move_taken;
        if(turn == 0) {
            move_taken = 4*9+4;
        } else {
            move_taken = get_best_move(b, last_move, player);//IDDFS(b, last_move, player);
        }
        apply_move(b, move_taken, player);
        int col = move_taken%9;
        int row = move_taken/9;
        cout << row << " " << col << endl;
        //cerr << "eval " << best_score << " time " << 1000*(std::clock() - tim)/CLOCKS_PER_SEC << "ms" << " nodes " << nodes << " chit " << chit << " depth " << depth << endl;
        player *= -1;
        turn += 1;
    }
}
/*
void bench() {
    board_t b;
    init_board(b);
    move_t last_move = NULL_MOVE;
    int player = 1;

    for(int depth = 6 ; depth < 12 ; depth++) {
        auto tim = std::clock();
        nodes = 0;
        chit = 0;
        cerr << "eval " << negamax(b, depth, player, -100000, 100000, last_move);
        auto time = 1000.0f * (std::clock() - tim)/CLOCKS_PER_SEC;
        auto npms = (float)(nodes) / time;
        cerr << " time " << time << " nodes " << nodes << " knps " << npms << " chit percentage " << 100*(float)(chit)/nodes << endl;
    }
}
*/

int play_tour(int me) {
    board_t b;
    init_board(b);
    move_t last_move = 4*9+4;
    apply_move(b, last_move, 1);
    int player = 1;

    while(!is_won(b)) {
        move_t m;
        auto t = now();
        if(player == me) {
            m = get_best_move(b, last_move, player);//IDDFS(b, last_move, player);
        } else {
            m = get_best_move(b, last_move, player);//play_tournament(b, last_move, player, zobrist_key);
        }
        /*
        if(now() - t > 50) {
            cerr << "BON CEST PAS OUF TOUT CA de " << (now() - t - 50);
            int a;
            cin >> a;
        }
        */
        apply_move(b, m, player);
        last_move = m;
        //print_wholeboard_filled(b);
        player = -player;
    }
    print_wholeboard_filled(b);

    int status = get_status(b);
    if(status == 0) {
        int forW = 0;
        int forB = 0;
        for(int i = 0 ; i < 9 ;i++) {
            int st = state_from_miniboard[b[i]];
            if(st == 2)
                forB ++;
            if(st == 1)
                forW ++;
        }
        if(forW > forB)
            return 1;
        if(forB > forW)
            return 2;
        return 0;
    }
    if(status == 2)
        status = -1;
    if(status == me)
        return 1;
    return -1;
}

void play_games(int n) {
    int scoreMe = 0;
    int scoreHim = 0;
    int me = 1;
    for(int i = 0 ; i < n ; i++) {
        int res = play_tour(me);
        if(res == 0) {
            scoreMe += 1;
            scoreHim += 1;
        }
        if(res == 1) {
            scoreMe += 2;
        }
        if(res == -1) {
            scoreHim += 2;
        }
        int color = me == 1?1:2;
        cerr << "Turn " << i << " I played as " << color << " scores - me: "<< scoreMe << " him: "<<scoreHim << endl;
        me *= -1;
    }
}

// Main

int main()
{
    initPrecalculations();
    //play_games(100);
    play_CG();
    //bench();
}
