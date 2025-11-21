#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <pthread.h>
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

    std::vector<int> ColoringCCAlgorithm(int num_threads);
};

// Thread data structure
struct ThreadData {
    CSRMatrix* csr;
    std::vector<int>* label;
    int start;
    int end;
    int thread_id;
    int* iterations;
    pthread_barrier_t* barrier;
    bool* global_changed;
    pthread_mutex_t* mutex;
};

// Worker thread function
void* worker_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    bool continue_loop = true;
    
    while (continue_loop) {
       
        //Thread 0 Resets flag at start of iteration
        if (data->thread_id == 0) {
            pthread_mutex_lock(data->mutex);
            *(data->global_changed) = false;
            pthread_mutex_unlock(data->mutex);
        }
        pthread_barrier_wait(data->barrier);
        
        bool local_changed = false;
        
        // Process assigned vertices
        for (int v = data->start; v < data->end; v++) {
            int min_label = (*data->label)[v];
            
            for (int k = data->csr->row_ptr[v]; k < data->csr->row_ptr[v+1]; k++) {
                int u = data->csr->col_ind[k];
                min_label = std::min(min_label, (*data->label)[u]);
            }
            
            if (min_label < (*data->label)[v]) {
                (*data->label)[v] = min_label;
                local_changed = true;
            }
        }
        
        // Update global changed flag if this thread changed anything
        if (local_changed) {
            pthread_mutex_lock(data->mutex);
            *(data->global_changed) = true;
            pthread_mutex_unlock(data->mutex);
        }
        
        // Wait for all threads to finish this iteration
        pthread_barrier_wait(data->barrier);
        
        // All threads read the decision
        pthread_mutex_lock(data->mutex);
        continue_loop = *(data->global_changed);
        pthread_mutex_unlock(data->mutex);
        
        // Thread 0 counts iterations
        if (data->thread_id == 0 ) {
            (*(data->iterations))++;
        }
        
        pthread_barrier_wait(data->barrier);
    }
    
    return nullptr;
}

std::vector<int> CSRMatrix::ColoringCCAlgorithm(int num_threads) {
    std::vector<int> label(num_vertices);
    
    // Initialize labels
    for (int v = 0; v < num_vertices; v++) {
        label[v] = v;
    }
    
    std::vector<pthread_t> threads(num_threads);
    std::vector<ThreadData> thread_data(num_threads);
    pthread_barrier_t barrier;
    pthread_mutex_t mutex;
    bool global_changed = false;
    int iterations = 0;
    
    pthread_barrier_init(&barrier, nullptr, num_threads);
    pthread_mutex_init(&mutex, nullptr);
    
    int chunk_size = (num_vertices + num_threads - 1) / num_threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].csr = this;
        thread_data[i].label = &label;
        thread_data[i].start = i * chunk_size;
        thread_data[i].end = std::min((i + 1) * chunk_size, num_vertices);
        thread_data[i].thread_id = i;
        thread_data[i].iterations = &iterations;
        thread_data[i].barrier = &barrier;
        thread_data[i].global_changed = &global_changed;
        thread_data[i].mutex = &mutex;
        
        pthread_create(&threads[i], nullptr, worker_thread, &thread_data[i]);
    }
    
    // Join threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], nullptr);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    
    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&mutex);
    
    std::cout << "Threads: " << num_threads << "\n";
    std::cout << "Converged after " << iterations << " iterations\n";
    std::cout << "Time: " << elapsed.count() << " seconds\n";
    
    return label;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Run: ./pthreads <mtx_file> <num_threads>\n";
        return 1;
    }
    
    std::string filename = argv[1];
    int num_threads = (argc >= 3) ? std::atoi(argv[2]) : 4;
    
    CSRMatrix CSR(filename);
    
    std::cout << "Vertices: " << CSR.num_vertices << ", Edges: " << CSR.nnz << "\n\n";
    
    std::vector<int> labels = CSR.ColoringCCAlgorithm(num_threads);
    
    std::sort(labels.begin(), labels.end());
    labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
    
    std::cout << "Connected Components: " << labels.size() << "\n";
    
    return 0;
}

// Compile: g++ -pthread -O3 pthreads.cpp -o pthreads
// Run: ./pthreads <mtx_file> <num_threads>
