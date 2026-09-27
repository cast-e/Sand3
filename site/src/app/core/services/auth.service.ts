import { Injectable, inject, signal } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, tap } from 'rxjs';

export interface User {
  id: string;
  username: string;
  role?: 'admin' | 'moderator' | 'user';
  is_admin?: boolean;
  created_at: string;
}

export interface AuthResponse {
  token: string;
  user: User;
}

function getAuthBaseUrl(): string {
  if (typeof window !== 'undefined') {
    if (window.location.port === '4200') {
      return (window as any).__SAND3_API_BASE_URL__ ? `${(window as any).__SAND3_API_BASE_URL__}/workshop/auth` : 'http://localhost:3000/api/workshop/auth';
    }
    return '/api/workshop/auth';
  }
  return '/api/workshop/auth';
}

@Injectable({
  providedIn: 'root'
})
export class AuthService {
  private http = inject(HttpClient);
  private baseUrl = getAuthBaseUrl();

  currentUser = signal<User | null>(null);
  token = signal<string | null>(localStorage.getItem('sand3_token'));

  isAuthenticated(): boolean {
    return !!this.currentUser();
  }

  isAdmin(): boolean {
    const u = this.currentUser();
    return Boolean(u && (u.role === 'admin' || u.is_admin));
  }

  isModerator(): boolean {
    if (this.isAdmin()) return true;
    const u = this.currentUser();
    return Boolean(u && (u.role === 'moderator' || u.role === 'admin'));
  }

  constructor() {
    if (this.token()) {
      this.checkAuth().subscribe({
        error: () => this.logout()
      });
    }
  }

  register(username: string, password: string): Observable<AuthResponse> {
    return this.http.post<AuthResponse>(`${this.baseUrl}/register`, { username, password }).pipe(
      tap((res) => this.setSession(res))
    );
  }

  login(username: string, password: string): Observable<AuthResponse> {
    return this.http.post<AuthResponse>(`${this.baseUrl}/login`, { username, password }).pipe(
      tap((res) => this.setSession(res))
    );
  }

  logout(): void {
    if (this.token()) {
      this.http
        .post(
          `${this.baseUrl}/logout`,
          {},
          {
            headers: { Authorization: `Bearer ${this.token()}` }
          }
        )
        .subscribe();
    }
    localStorage.removeItem('sand3_token');
    this.token.set(null);
    this.currentUser.set(null);
  }

  checkAuth(): Observable<{ user: User }> {
    return this.http
      .get<{ user: User }>(`${this.baseUrl}/me`, {
        headers: { Authorization: `Bearer ${this.token()}` }
      })
      .pipe(tap((res) => this.currentUser.set(res.user)));
  }

  private setSession(auth: AuthResponse): void {
    localStorage.setItem('sand3_token', auth.token);
    this.token.set(auth.token);
    this.currentUser.set(auth.user);
  }
}
