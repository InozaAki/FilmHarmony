#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <set>
#include "httplib.h"
#include "json.hpp"
#include <stdlib.h>

const std::string tmdb_api_key = "05ef1d094082f4d4a01f5e71fb7ba0d8";  // Reemplazar con tu clave de API

// Estructura para almacenar detalles de una película
struct Movie {
    std::string title;
    std::set<std::string> genres;
    std::vector<std::string> cast;       // Lista de miembros del elenco
    std::string director;                // Nombre del director
    std::vector<std::string> keywords;   // Palabras clave asociadas con la película

   double ranking;

};

//Algoritmo de Ordenamiento
int partition(std::vector<Movie>&, int, int);
void quicksort(std::vector<Movie>&, int, int);
void sortMovies(std::vector<Movie>&);

// Función para obtener detalles de una película desde TMDb
Movie fetchMovieDetails(int movie_id) {
    httplib::Client cli("api.themoviedb.org");

    // Obtener los detalles básicos de la película
    std::string basicDetailsUrl = "/3/movie/" + std::to_string(movie_id) + "?api_key=" + tmdb_api_key;
    auto basicRes = cli.Get(basicDetailsUrl.c_str());
    if (!(basicRes && basicRes->status == 200)) {
        std::cerr << "Error al obtener los detalles básicos de la película." << std::endl;
        return Movie{};  // Devolver un objeto Movie vacío
    }
    nlohmann::json basicData = nlohmann::json::parse(basicRes->body);

    Movie movie;
    movie.title = basicData["title"].get<std::string>();
    movie.ranking = basicData["vote_average"].get<double>();
    for (const auto& genre : basicData["genres"]) {
        movie.genres.insert(genre["name"].get<std::string>());
    }

    // Obtener detalles adicionales como el elenco y las palabras clave
    std::string creditsUrl = "/3/movie/" + std::to_string(movie_id) + "/credits?api_key=" + tmdb_api_key;
    auto creditsRes = cli.Get(creditsUrl.c_str());
    if (creditsRes && creditsRes->status == 200) {
        nlohmann::json creditsData = nlohmann::json::parse(creditsRes->body);

        for (const auto& castMember : creditsData["cast"]) {
            movie.cast.push_back(castMember["name"].get<std::string>());
        }

        for (const auto& crewMember : creditsData["crew"]) {
            if (crewMember["job"].get<std::string>() == "Director") {
                movie.director = crewMember["name"].get<std::string>();
                break;  // Suponiendo un solo director por película
            }
        }
    }

    std::string keywordsUrl = "/3/movie/" + std::to_string(movie_id) + "/keywords?api_key=" + tmdb_api_key;
    auto keywordsRes = cli.Get(keywordsUrl.c_str());
    if (keywordsRes && keywordsRes->status == 200) {
        nlohmann::json keywordsData = nlohmann::json::parse(keywordsRes->body);

        for (const auto& keyword : keywordsData["keywords"]) {
            movie.keywords.push_back(keyword["name"].get<std::string>());
        }
    }

    return movie;
}

// Función para calcular la similitud de géneros entre dos películas
int calculateGenreSimilarity(const Movie& movie1, const Movie& movie2) {
    int count = 0;
    for (const auto& genre : movie1.genres) {
        if (movie2.genres.find(genre) != movie2.genres.end()) {
            count++;
        }
    }
    return count;
}

// Función para calcular la similitud del elenco entre dos películas
int calculateCastSimilarity(const Movie& movie1, const Movie& movie2) {
    std::set<std::string> cast1(movie1.cast.begin(), movie1.cast.end());
    std::set<std::string> cast2(movie2.cast.begin(), movie2.cast.end());
    int count = std::count_if(cast1.begin(), cast1.end(), 
                              [&](const std::string& member){ return cast2.find(member) != cast2.end(); });
    return count;
}

// Función para calcular la similitud del director entre dos películas
int calculateDirectorSimilarity(const Movie& movie1, const Movie& movie2) {
    return movie1.director == movie2.director ? 1 : 0;
}

// Función para calcular la similitud de palabras clave entre dos películas
int calculateKeywordSimilarity(const Movie& movie1, const Movie& movie2) {
    std::set<std::string> keywords1(movie1.keywords.begin(), movie1.keywords.end());
    std::set<std::string> keywords2(movie2.keywords.begin(), movie2.keywords.end());
    int count = std::count_if(keywords1.begin(), keywords1.end(), 
                              [&](const std::string& keyword){ return keywords2.find(keyword) != keywords2.end(); });
    return count;
}

// Función para calcular la similitud multifactorial entre dos películas
int calculateMultiFactorSimilarity(const Movie& movie1, const Movie& movie2) {
    int similarityScore = 0;

    // Calcular similitud basada en géneros
    similarityScore += calculateGenreSimilarity(movie1, movie2);

    // Calcular similitud basada en el elenco
    similarityScore += calculateCastSimilarity(movie1, movie2);

    // Calcular similitud basada en el director
    similarityScore += calculateDirectorSimilarity(movie1, movie2);

    // Calcular similitud basada en palabras clave
    similarityScore += calculateKeywordSimilarity(movie1, movie2);

    return similarityScore;
}

// Función para obtener películas similares desde TMDb
std::vector<Movie> fetchSimilarMovies(int movie_id) {
    std::vector<Movie> similarMovies;
    httplib::Client cli("api.themoviedb.org");
    std::string url = "/3/movie/" + std::to_string(movie_id) + "/similar?api_key=" + tmdb_api_key;

    auto res = cli.Get(url.c_str());
    if (res && res->status == 200) {
        nlohmann::json jsonData = nlohmann::json::parse(res->body);
        for (const auto& item : jsonData["results"]) {
            int similarMovieId = item["id"].get<int>();
            // Utiliza fetchMovieDetails para obtener detalles completos de cada película similar
            Movie similarMovie = fetchMovieDetails(similarMovieId);
            similarMovies.push_back(similarMovie);
        }
    } else {
        std::cerr << "Error al obtener películas similares." << std::endl;
    }
    
    return similarMovies;
}

// Función para calcular los pesos de los géneros
std::unordered_map<std::string, int> calculateGenreWeights(const std::vector<Movie>& movies) {
    std::unordered_map<std::string, int> genreFrequency;
    for (const auto& movie : movies) {
        for (const auto& genre : movie.genres) {
            genreFrequency[genre]++;
        }
    }

    std::unordered_map<std::string, int> genreWeights;
    for (const auto& genre : genreFrequency) {
        genreWeights[genre.first] = 1 + (10 / genre.second); // Ajuste de ponderación
    }

    return genreWeights;
}

// Función para calcular la similitud mejorada de géneros entre dos películas
int enhancedGenreSimilarity(const Movie& movie1, const Movie& movie2, const std::unordered_map<std::string, int>& genreWeights) {
    int similarity = 0;
    for (const auto& genre : movie1.genres) {
        if (movie2.genres.count(genre)) {
            similarity += genreWeights.count(genre) ? genreWeights.at(genre) : 1;
        }
    }
    return similarity;
}

// Función para calcular la similitud multifactorial mejorada entre dos películas
int calculateMultiFactorSimilarity(const Movie& movie1, const Movie& movie2, const std::unordered_map<std::string, int>& genreWeights) {
    int similarityScore = 0;
    similarityScore += enhancedGenreSimilarity(movie1, movie2, genreWeights);
    similarityScore += calculateCastSimilarity(movie1, movie2);
    similarityScore += calculateDirectorSimilarity(movie1, movie2);
    similarityScore += calculateKeywordSimilarity(movie1, movie2);
    return similarityScore;
}

// Función para obtener películas adicionales basadas en la película elegida
std::vector<Movie> fetchAdditionalMovies(const Movie& chosenMovie) {
    // Obtener películas adicionales, por ejemplo, las películas mejor valoradas en los mismos géneros que la película elegida.
    std::vector<Movie> additionalMovies;
    return additionalMovies;
}

// Función para combinar recomendaciones
std::vector<Movie> blendRecommendations(const std::vector<Movie>& tmdbRecommendations, const std::vector<Movie>& additionalMovies) {
    std::vector<Movie> blendedMovies = tmdbRecommendations;
    blendedMovies.insert(blendedMovies.end(), additionalMovies.begin(), additionalMovies.end());
    return blendedMovies;
}

// Función para recomendar películas
std::vector<Movie> recommendMovies(const std::vector<Movie>& blendedMovies, const Movie& chosenMovie) {
    auto genreWeights = calculateGenreWeights(blendedMovies);

    std::vector<std::pair<Movie, int>> weightedMovies;
    for (const auto& movie : blendedMovies) {
        int similarity = calculateMultiFactorSimilarity(chosenMovie, movie, genreWeights);
        weightedMovies.push_back({movie, similarity});
    }

    std::stable_sort(weightedMovies.begin(), weightedMovies.end(), 
        [](const std::pair<Movie, int>& a, const std::pair<Movie, int>& b) {
            return a.second > b.second;
        });

    const int SIMILARITY_THRESHOLD = 2;  // Establecer esto según las pruebas
    std::vector<Movie> sortedMovies;
    for (const auto& pair : weightedMovies) {
        if (pair.second >= SIMILARITY_THRESHOLD) {
            sortedMovies.push_back(pair.first);
        }
    }

    return sortedMovies;
}

int partition(std::vector<Movie>& Movies, int low, int high) {
    int pivote = Movies[high].ranking;
    int i = low - 1;
    for (int j = low; j <= high - 1; j++) {
        if (Movies[j].ranking > pivote || Movies[j].ranking == pivote) {
            i++;
            std::swap(Movies[i], Movies[j]);
        }
    }
    std::swap(Movies[i + 1], Movies[high]);
    return (i + 1);
}

void quicksort(std::vector<Movie>& Movies, int low, int high){
  if(low < high){
    int par = partition(Movies, low, high);
    quicksort(Movies, low, par-1);
    quicksort(Movies, par+1, high);
  }
}
void sortMovies(std::vector<Movie>& Movies){
  quicksort(Movies, 0, Movies.size()-1);
}


int main() {
    std::vector<int> movieIds = {550, 872585}; // Lista de IDs de películas

    for (int movieId : movieIds) {
        // Obtener detalles de la película principal
        Movie mainMovie = fetchMovieDetails(movieId);

        // Mostrar el título de la película principal
        std::cout << "\n\n\nRecomendaciones para '" << mainMovie.title << "':" << std::endl;

        // Obtener y mostrar recomendaciones
        std::vector<Movie> recommendations = fetchSimilarMovies(movieId);
        sortMovies(recommendations);
        for (const auto& movie : recommendations) {
            std::cout << "Título: " << movie.title << "\nGéneros: ";
            for (const auto& genre : movie.genres) {
                std::cout << genre << ", ";
            }
            std::cout << "\nVote Rate: " << movie.ranking;
            std::cout << "\n\nElenco: ";
            for (const auto& castMember : movie.cast) {
                std::cout << castMember << ", ";
            }
            std::cout << "\n\nDirector: " << movie.director << "\n\nPalabras clave: ";
            for (const auto& keyword : movie.keywords) {
                std::cout << keyword << ", ";
            }
            std::cout << "\n--------------------------------\n";
        }
    }

    return 0;
}

