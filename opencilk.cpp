#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <chrono>

struct CSRMatrix
{   
    std::vector<int> row_ptr;
    std::vector<int> col_ind;
    int num_vertices = 0;
    int nnz = 0;
    
    CSRMatrix(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << filename << "\n";
            exit(1);
        }
        std::string header;
        std::getline(file, header);
        std::istringstream hss(header);
        std::string mm, object, format, field, symmetry;
        hss >> mm >> object >> format >> field >> symmetry;

        bool is_symmetric = (symmetry == "symmetric");

        std::string line;
        while (std::getline(file, line)) {
            if (line[0] != '%') break;
        }

        int rows, cols;
        std::istringstream iss(line);
        iss >> rows >> cols >> nnz;

        if (rows != cols) {
            std::cerr << "Error: Matrix must be square for an adjacency matrix.\n";
            exit(1);
        }

        num_vertices = rows;
        int original_nnz = nnz;

        // Two-pass approach - First pass: count degrees
        std::streampos data_pos = file.tellg();
        std::vector<int> degree(num_vertices, 0);

        for (int i = 0; i < original_nnz; i++) {
            int row, col;
            file >> row >> col;
            if (field != "pattern") {
                double val;
                file >> val;
            }
            row--; col--;
            degree[row]++;
            if (is_symmetric && row != col) {
                degree[col]++;
            }
        }

        // Build row_ptr from degrees
        row_ptr.assign(num_vertices + 1, 0);
        for (int i = 0; i < num_vertices; i++) {
            row_ptr[i + 1] = row_ptr[i] + degree[i];
        }

        nnz = row_ptr[num_vertices];
        col_ind.resize(nnz);

        // Second pass: fill col_ind
        file.clear();
        file.seekg(data_pos);
        std::vector<int> current_pos = row_ptr;

        for (int i = 0; i < original_nnz; i++) {
            int row, col;
            file >> row >> col;
            if (field != "pattern") {
                double val;
                file >> val;
            }
            row--; col--;
            col_ind[current_pos[row]++] = col;
            if (is_symmetric && row != col) {
                col_ind[current_pos[col]++] = row;
            }
        }
        
        file.close();
    }

    std::vector<int> ColoringCCAlgorithm() {
        std::vector<int> label(num_vertices);
        std::vector<bool> local_changed(num_vertices);

        cilk_for (int v = 0; v < num_vertices; v++) {
            label[v] = v;
        }

        bool changed;
        int iterations = 0;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        do {
            changed = false;
            iterations++;
            
            cilk_for (int v = 0; v < num_vertices; v++) {
                local_changed[v] = false;
            }
            
            cilk_for (int v = 0; v < num_vertices; v++) {
                int min_label = label[v];
                
                for (int k = row_ptr[v]; k < row_ptr[v+1]; k++) {
                    int u = col_ind[k];
                    min_label = std::min(min_label, label[u]);
                }
                
                if (min_label < label[v]) {
                    label[v] = min_label;
                    local_changed[v] = true;
                }
            }
            
            
            // Check if any vertex changed its label
            for (int v = 0; v < num_vertices; v++) {
                if (local_changed[v]) {
                    changed = true;
                    break;
                }
            }
            
        } while (changed);
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        
        std::cout << "Converged after " << iterations << " iterations\n";
        std::cout << "Time: " << elapsed.count() << " seconds\n";
        
        return label;
    }
};

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Run: CILK_NWORKERS=N ./opencilk <mtx_file>\n";
        return 1;
    }
    
    std::string filename = argv[1];
    
    
    CSRMatrix CSR(filename);
    
    std::cout << "Vertices: " << CSR.num_vertices << ", Edges: " << CSR.nnz << "\n\n";
    
    std::vector<int> labels = CSR.ColoringCCAlgorithm();
    
    std::sort(labels.begin(), labels.end());
    labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
    
    std::cout << "Connected Components: " << labels.size() << "\n";
    
    return 0;
}

// Compile: clang++ -fopencilk -O3 opencilk.cpp -o opencilk
// Run with default workers: ./opencilk <mtx_file>
// Set number of workers: CILK_NWORKERS=N ./cilk <mtx_file>