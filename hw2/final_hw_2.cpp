#include <iostream>
#include <vector>
#include <fstream>
// #include "NetPBM-10.68.1-win-14.11.01\NetPBM-10.68\include\netpbm\pm_config.h"
#include <thread>
#include <time.h>
#include <chrono>
#include <mutex>
#include <atomic>

struct game{
private:
    std::vector<std::vector<int32_t>> table;
    uint32_t number_cols;
    uint32_t number_rows;

public:
    void read_initial_state(std::ifstream& in,const std::string& file_name, std::vector<std::vector<int32_t>>& result){
        in.open(file_name);

        std::string format;
        //read format
        std::getline(in, format);

        uint32_t size_rows, size_cols; 
        //read sizes as string
        std::string sizes;
        std::getline(in, sizes);
        //extract row size and col size
        uint32_t delimiter_pos = sizes.find(" ");
        std::string first = sizes.substr(0, delimiter_pos);
        std::string second = sizes.substr(delimiter_pos + 1, sizes.size());

        //convert sizes to int
        size_rows = std::stoi(first);
        size_cols = std::stoi(second);

        this->number_cols = size_cols;
        this->number_rows = size_rows;

        //create matrix
        std::vector<std::vector<int32_t>> matrix(size_rows);
        uint32_t current_row = 0;
        //read matrix from file

        while (!in.eof())
        {
            std::string row;
            std::getline(in, row);

            if (row.size() != size_cols)
            {
                std::cout << "Please build the image with the correct number of rows and columns!!!!!!\n";
                return;
            }
            for (size_t i = 0; i < row.size(); i++)
            {
                matrix[current_row].push_back(row[i] - '0');
            }
            current_row++;
        }
        
        //close file
        in.close();

        result = matrix;
        this->table = result;
    }

    void write_final_state(std::ofstream& out, std::vector<std::vector<int32_t>>& final_state,const std::string& file_name){

        out.open(file_name);

        out << "P1\n";
        out << final_state.size() << " " << final_state[0].size() << "\n";
        for (uint32_t i = 0; i < final_state.size(); i++)
        {
            for (uint32_t j = 0; j < final_state[0].size(); j++)
            {
                out << final_state[i][j];
            }
            out << "\n";
        }
        out.close();
    }
    uint32_t count_alive_neighbours(const int32_t& index_row, const int32_t& index_col,const std::vector<std::vector<int32_t>>& table){
        uint32_t counter = 0;
        int32_t size_rows = table.size();
        int32_t size_cols = table[0].size();

        //check all 8 neighbours sequentially
        if(index_row + 1 < size_rows && table[index_row + 1][index_col] == 1){counter++;}
        if(index_col + 1 < size_cols && table[index_row][index_col + 1] == 1){counter++;}
        if(index_row - 1 >= 0 && table[index_row - 1][index_col] == 1){counter++;}
        if(index_col - 1 >= 0 && table[index_row][index_col - 1] == 1){counter++;}
        if(index_row - 1 >= 0 && index_col - 1 >= 0 && table[index_row - 1][index_col - 1] == 1){counter++;}
        if(index_row - 1 >= 0 && index_col + 1 < size_cols && table[index_row - 1][index_col + 1] == 1){counter++;}
        if(index_row + 1 < size_rows && index_col - 1 >= 0 && table[index_row + 1][index_col - 1] == 1){counter++;} 
        if(index_row + 1 < size_rows && index_col + 1 < size_cols && table[index_row + 1][index_col + 1] == 1){counter++;}

        return counter;
    }
    void advance_generations_threading2(std::vector<std::vector<int32_t>>& table, int number_threads,int thread_index){
        //temp matrix to make changes which will not affect next rows and columns
        std::mutex mutex;
        mutex.lock();
        std::vector<std::vector<int32_t>> result(table);
        for (uint32_t i = thread_index; i < table.size(); i+=number_threads)
        {
            for (uint32_t j = thread_index; j < table[0].size(); j+=number_threads)
            {
                uint32_t current_alive_count = count_alive_neighbours(i, j, table);
                if (table[i][j] == 1)
                {
                    if(current_alive_count < 2){
                        result[i][j] = 0;
                    }
                    else if(current_alive_count > 3){
                        result[i][j] = 0;
                    }
                    else if(current_alive_count == 2 || current_alive_count == 3){
                        result[i][j] = 1;
                    }
                }
                else if (table[i][j] == 0 && current_alive_count == 3){
                    result[i][j] = 1;
                }
                else{result[i][j] = 0;}
                this->table[i][j] = result[i][j];
            }
        }
        mutex.unlock();
    }

    std::vector<std::vector<int32_t>> get_table() const{
        return this->table;
    }
};
int main(){
    game g;

    uint32_t number_threads;
    std::cout << "Enter number of threads on which you want to execute the game (1,2,3,4): ";
    std::cin >> number_threads;

    if (number_threads > std::thread::hardware_concurrency() / 2){number_threads = std::thread::hardware_concurrency() / 2;}

    std::ifstream in;

    std::vector<std::thread> threads;
    std::vector<std::vector<int32_t>> table, res;
    std::ofstream f;



    g.read_initial_state(in, "diehard.pbm", table);

    uint32_t number_iters;
    std::cout << "Enter number of itierations: ";
    std::cin >> number_iters;

    for (uint32_t j = 0; j < number_iters; j++)
    {
        threads.erase(threads.begin(), threads.end());
        for (uint32_t i = 0; i < number_threads; i++)
        {
            threads.push_back(std::thread(g.advance_generations_threading2, std::ref(table), number_threads, i));
            threads[i].join();
        }
    }
    
    table = g.get_table();
    g.write_final_state(f, table, "result_threading_3_threads.pbm");
    return 0;
}